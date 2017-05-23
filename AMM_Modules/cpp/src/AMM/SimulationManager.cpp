#include "SimulationManager.h"

using namespace AMM;
using namespace AMM::Simulation;
using namespace DDS;
using namespace std;
using namespace std::chrono;

SimulationManager::SimulationManager() :
		m_thread() {

	char partition_name[] = "AMM";
	char tick_topic_name[] = "Tick";

	mgr.createParticipant(partition_name);
	TickTypeSupport_var tt = new TickTypeSupport();
	mgr.registerType(tt.in());
	mgr.createTopic(tick_topic_name);

	//create Publisher
	mgr.createPublisher();

	// create DataWriter :
	mgr.createWriters();

	// Publish Events
	dwriter = mgr.getWriter();
	TickWriter = TickDataWriter::_narrow(dwriter.in());

	pauseTick.frame = -2;
	shutdownTick.frame = -1;

	m_runThread = false;

}

SimulationManager::~SimulationManager() {

}

void SimulationManager::StartSimulation() {
	if (!m_runThread) {
		m_runThread = true;
		m_thread = std::thread(&SimulationManager::TickLoop, this);
	}
}

void SimulationManager::StopSimulation() {
	if (m_runThread) {
		m_runThread = false;
		// ReturnCode_t status = TickWriter->write(pauseTick, DDS::HANDLE_NIL);
		// checkStatus(status, "TickDataWriter::write");
		m_thread.detach();
	}
}

int SimulationManager::GetTickCount() {
	return tickCount;
}

bool SimulationManager::isRunning() {
	return m_runThread;
}

void SimulationManager::SetSampleRate(int rate) {
	sampleRate = rate;
}

int SimulationManager::GetSampleRate() {
	return sampleRate;
}

void SimulationManager::TickLoop() {
	using frames = duration<int64_t, ratio<1, 50>>;    // 50hz
	auto nextFrame = system_clock::now();
	auto lastFrame = nextFrame - frames { 1 };

	while (m_runThread) {
		this_thread::sleep_until(nextFrame);
		m_mutex.lock();
		tick.frame = tickCount++;
		ReturnCode_t status = TickWriter->write(tick, DDS::HANDLE_NIL);
		checkStatus(status, "TickDataWriter::write");
		lastFrame = nextFrame;
		nextFrame += frames { 1 };
		m_mutex.unlock();
	}
}

void SimulationManager::Cleanup() {
	/* Remove the DataWriters */
	mgr.deleteWriter(dwriter.in());

	/* Remove the Publisher. */
	mgr.deletePublisher();

}

void SimulationManager::Shutdown() {
	if (m_runThread) {
		m_runThread = false;
		std::this_thread::sleep_for(std::chrono::seconds(1));

		ReturnCode_t status = TickWriter->write(shutdownTick, DDS::HANDLE_NIL);
		checkStatus(status, "TickDataWriter::write");
		m_thread.detach();
	}

	m_thread.~thread();
	std::terminate();
}

