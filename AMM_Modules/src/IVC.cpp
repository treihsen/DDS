#include "stdafx.h"

#include <string>
#include <iostream>
#include <algorithm>

#include <boost/algorithm/string.hpp>

#include <thirdparty/tixml2cx.h>

#include "AMM/DDS_Manager.h"
#include "spi_proto.h"

#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include <fcntl.h>    /* For O_RDWR */

using namespace std;
using namespace std::literals::string_literals;


// Daemonize by default
int daemonize = 1;


const string loadScenarioPrefix = "LOAD_SCENARIO:";
const string haltingString = "HALTING_ERROR";

//variables that are sent to the tiny
float operating_pressure;
//variables that are received from the tiny
bool pressurized = false;
float total_flow = 0, last_flow_change = 0;

bool have_pressure = 0;
bool send_status = false;
AMM::Capability::status_values current_status;

static const char *device = "/dev/spidev0.0";
static uint8_t mode;
static uint8_t bits = 8;
static uint32_t speed = 1 << 23;
static uint16_t delay;

//TODO centralize
#define TRANSFER_SIZE 36

int spi_transfer(int fd, const unsigned char *tx_buf, unsigned char *rx_buf, __u32 buflen) {
    int ret;
    struct spi_ioc_transfer tr = {
            tx_buf : (unsigned long) tx_buf,
            rx_buf : (unsigned long) rx_buf,
            len : TRANSFER_SIZE, speed_hz : speed,
            delay_usecs : delay, bits_per_word : bits,
    };

    ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1)
        perror("can't send spi message");
    return ret;
}

struct spi_state spi_state;

// @TODO: Need to adjust this to the IVC config
void ProcessConfig(const std::string configContent) {

    tinyxml2::XMLDocument doc;
    doc.Parse(configContent.c_str());

    tinyxml2::XMLHandle docHandle(&doc);

    tinyxml2::XMLElement *entry = docHandle.FirstChildElement("AMMConfiguration").ToElement();

    tinyxml2::XMLElement *entry2 = entry->FirstChildElement("scenario")->ToElement();

    tinyxml2::XMLElement *entry3 = entry2->FirstChildElement("capabilities")->ToElement();

    tinyxml2::XMLElement *entry4 = entry3->FirstChildElement("capability")->ToElement();


    //scan for capability name=ivc
    while (entry4) {
        if (!strcmp(entry4->ToElement()->Attribute("name"), "ivc")) break;

        auto v = entry4->ToElement()->NextSibling();
        if (v) {
            entry4 = v->ToElement();
        } else break;
    }
    if (!entry4) {
        perror("[IVC] cfg data didn't contain <capability name=ivc>");
        return;
    }

    //scan for data name=operating pressure
    tinyxml2::XMLElement *entry5 = entry4->FirstChildElement("configuration_data")->ToElement();

    while (entry5) {
        tinyxml2::XMLElement *entry5_1 = entry5->FirstChildElement("data")->ToElement();
        if (!strcmp(entry5_1->ToElement()->Attribute("name"), "operating_pressure")) {
            operating_pressure = entry5_1->ToElement()->FloatAttribute("value");
            have_pressure = 1;
            unsigned char spi_send[8];
            spi_send[0] = 1;
            spi_send[1] = 0;
            memcpy(spi_send + 4, &operating_pressure, 4);
            spi_proto_send_msg(&spi_state, spi_send, 8);
            break;
        }
        auto v = entry5->ToElement()->NextSibling();
        if (v) {
            entry5 = v->ToElement();
        } else break;
    }

    if (!entry5) {
        perror("[FLUIDMGR] cfg data didn't contain <data name=??>");
    }

}


class IVCListener : public ListenerInterface {

    void onNewCommandData(AMM::PatientAction::BioGears::Command c) override {
        // We received configuration which we need to push via SPI
        if (!c.message().compare(0, sysPrefix.size(), sysPrefix)) {
            std::string value = c.message().substr(sysPrefix.size());
	    if (value.compare("START_SIM") == 0) {

	    } else if (value.compare("STOP_SIM") == 0) {

	    } else if (value.compare("PAUSE_SIM") == 0) {

	    } else if (value.compare("RESET_SIM") == 0) {

	    } else if (!value.compare(0, loadScenarioPrefix.size(), loadScenarioPrefix)) {
                std::string scene = value.substr(loadScenarioPrefix.size());
                boost::algorithm::to_lower(scene);
                ostringstream static_filename;
                static_filename << "mule1/module_configuration_static/" << scene << "_ivc_module.xml";
                std::ifstream ifs(static_filename.str());
                std::string configContent((std::istreambuf_iterator<char>(ifs)),
                                          (std::istreambuf_iterator<char>()));

                ifs.close();

                ProcessConfig(configContent);

                // These should be sent when a status change is received via spi.
                // We'll force them for now.
                send_status = true;
                current_status = OPERATIONAL;
            }
        }
    }
};


static void show_usage(const std::string &name) {
    cerr << "Usage: " << name << "\nOptions:\n"
         << "\t-h,--help\t\tShow this help message\n" << endl;
}

int main(int argc, char *argv[]) {
    cout << "=== [IVC_Module] Ready ..." << endl;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-h") || (arg == "--help")) {
            show_usage(argv[0]);
            return 0;
        }

        if (arg == "-d") {
            daemonize = 1;
        }
    }

    int spi_fd = open(device, O_RDWR);
    unsigned char recvbuf[TRANSFER_SIZE];
    unsigned char sendbuf[TRANSFER_SIZE] = {};
    struct spi_state *s = &spi_state;
    spi_proto_initialize(s);


    const char *nodeName = "AMM_IVC";
    std::string nodeString(nodeName);
    auto *mgr = new DDS_Manager(nodeName);

    auto *command_sub_listener = new DDS_Listeners::CommandSubListener();
    auto *config_sub_listener = new DDS_Listeners::ConfigSubListener();

    IVCListener ivcl;
    command_sub_listener->SetUpstream(&ivcl);
    config_sub_listener->SetUpstream(&ivcl);

    auto *pub_listener = new DDS_Listeners::PubListener();

    mgr->InitializeSubscriber(AMM::DataTypes::commandTopic, AMM::DataTypes::getCommandType(), command_sub_listener);
    mgr->InitializeSubscriber(AMM::DataTypes::configurationTopic, AMM::DataTypes::getConfigurationType(),
                              config_sub_listener);

    Publisher *command_publisher = mgr->InitializePublisher(AMM::DataTypes::commandTopic,
                                                            AMM::DataTypes::getCommandType(), pub_listener);


    // Publish module configuration once we've set all our publishers and listeners
    // This announces that we're available for configuration

    mgr->PublishModuleConfiguration(
            nodeString,
            "Entropic",
            "ivc_module",
            "00001",
            "0.0.1",
            mgr->GetCapabilitiesAsString("mule1/module_capabilities/ivc_module_capabilities.xml")
    );

    int count = 0;
    bool closed = 0;
    while (!closed) {
        int ret = spi_proto_prep_msg(s, sendbuf, TRANSFER_SIZE);
        //TODO currently no code sends the start command

        //do SPI communication
        int spi_tr_res = spi_transfer(spi_fd, sendbuf, recvbuf, TRANSFER_SIZE);

        struct spi_packet pack;
        memcpy(&pack, recvbuf, TRANSFER_SIZE);
        //IVC spi response format:
        //[VALID|PRESSURIZED|NO|NO| F|LO|A|T|]
        if (recvbuf[0]) {
            pressurized = recvbuf[1];
	    cout << "\tPressurized: " << pressurized << endl;
            float total_flow_new;
            memcpy(&total_flow_new, &recvbuf[4], 4);
            last_flow_change = total_flow_new - total_flow;
            total_flow = total_flow_new;
	    cout << "\t Total Flow: " << endl;
        }

	
        //TODO send message on pressurization
        
        //TODO send message to biogears about flow
	
	
	
        if (send_status) {
            cout << "[IVC] Setting status to " << current_status << endl;
            send_status = false;
            mgr->SetStatus(nodeString, current_status);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ++count;
    }

    cout << "=== [IVC] Simulation stopped." << endl;

    return 0;

}

