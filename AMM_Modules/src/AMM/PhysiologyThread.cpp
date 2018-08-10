#include "PhysiologyThread.h"

using namespace std;
using namespace AMM::Physiology;

std::vector<std::string> explode(const std::string &delimiter, const std::string &str) {
    std::vector<std::string> arr;

    int strleng = str.length();
    int delleng = delimiter.length();
    if (delleng == 0)
        return arr;//no change

    int i = 0;
    int k = 0;
    while (i < strleng) {
        int j = 0;
        while (i + j < strleng && j < delleng && str[i + j] == delimiter[j])
            j++;
        if (j == delleng)//found delimiter
        {
            arr.push_back(str.substr(k, i - k));
            i += delleng;
            k = i;
        } else {
            i++;
        }
    }
    arr.push_back(str.substr(k, i - k));
    return arr;
};

namespace AMM {
    std::vector<std::string> PhysiologyThread::highFrequencyNodes;
    std::map<std::string, double (PhysiologyThread::*)()> PhysiologyThread::nodePathTable;

    PhysiologyThread::PhysiologyThread(const std::string &logFile) {
        m_pe = CreateBioGearsEngine(logFile);
        PopulateNodePathTable();
        m_runThread = false;
    }

    PhysiologyThread::~PhysiologyThread() {
        m_runThread = false;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    void PhysiologyThread::PreloadSubstances() {
        sodium = m_pe->GetSubstanceManager().GetSubstance("Sodium");
        glucose = m_pe->GetSubstanceManager().GetSubstance("Glucose");
        creatinine = m_pe->GetSubstanceManager().GetSubstance("Creatinine");
        calcium = m_pe->GetSubstanceManager().GetSubstance("Calcium");
        hemoglobin = m_pe->GetSubstanceManager().GetSubstance("Hemoglobin");
        bicarbonate = m_pe->GetSubstanceManager().GetSubstance("Bicarbonate");
        albumin = m_pe->GetSubstanceManager().GetSubstance("Albumin");
        CO2 = m_pe->GetSubstanceManager().GetSubstance("CarbonDioxide");
        N2 = m_pe->GetSubstanceManager().GetSubstance("Nitrogen");
        O2 = m_pe->GetSubstanceManager().GetSubstance("Oxygen");
        CO = m_pe->GetSubstanceManager().GetSubstance("CarbonMonoxide");
    };

    void PhysiologyThread::PreloadCompartments() {
        carina = m_pe->GetCompartments().GetGasCompartment(BGE::PulmonaryCompartment::Carina);
        leftLung = m_pe->GetCompartments().GetGasCompartment(BGE::PulmonaryCompartment::LeftLung);
        rightLung = m_pe->GetCompartments().GetGasCompartment(BGE::PulmonaryCompartment::RightLung);

    }

    void PhysiologyThread::PopulateNodePathTable() {
        highFrequencyNodes.clear();
        nodePathTable.clear();

        // Legacy values
        nodePathTable["ECG"] = &PhysiologyThread::GetECGWaveform;
        nodePathTable["HR"] = &PhysiologyThread::GetHeartRate;
        nodePathTable["SIM_TIME"] = &PhysiologyThread::GetSimulationTime;
        nodePathTable["EXIT"] = &PhysiologyThread::GetShutdownMessage;

        // Cardiovascular System
        nodePathTable["Cardiovascular_HeartRate"] = &PhysiologyThread::GetHeartRate;
        nodePathTable["Cardiovascular_BloodVolume"] = &PhysiologyThread::GetBloodVolume;
        nodePathTable["Cardiovascular_Arterial_Pressure"] = &PhysiologyThread::GetArterialPressure;
        nodePathTable["Cardiovascular_Arterial_Mean_Pressure"] = &PhysiologyThread::GetMeanArterialPressure;
        nodePathTable["Cardiovascular_Arterial_Systolic_Pressure"] = &PhysiologyThread::GetArterialSystolicPressure;
        nodePathTable["Cardiovascular_Arterial_Diastolic_Pressure"] = &PhysiologyThread::GetArterialDiastolicPressure;
        nodePathTable["Cardiovascular_CentralVenous_Mean_Pressure"] = &PhysiologyThread::GetMeanCentralVenousPressure;
        nodePathTable["Cardiovascular_CardiacOutput"] = &PhysiologyThread::GetCardiacOutput;

        // Respiratory System
        nodePathTable["Respiratory_Respiration_Rate"] = &PhysiologyThread::GetRespirationRate;
        nodePathTable["Respiration_EndTidalCarbonDioxide"] = &PhysiologyThread::GetEndTidalCarbonDioxideFraction;
        nodePathTable["Respiratory_Tidal_Volume"] = &PhysiologyThread::GetTidalVolume;
        nodePathTable["Respiratory_LungTotal_Volume"] = &PhysiologyThread::GetTotalLungVolume;
        nodePathTable["Respiratory_LeftPleuralCavity_Volume"] = &PhysiologyThread::GetLeftPleuralCavityVolume;
        nodePathTable["Respiratory_LeftLung_Volume"] = &PhysiologyThread::GetLeftLungVolume;
        nodePathTable["Respiratory_LeftAlveoli_BaseCompliance"] = &PhysiologyThread::GetLeftAlveoliBaselineCompliance;
        nodePathTable["Respiratory_RightPleuralCavity_Volume"] = &PhysiologyThread::GetRightPleuralCavityVolume;
        nodePathTable["Respiratory_RightLung_Volume"] = &PhysiologyThread::GetRightLungVolume;
        nodePathTable["Respiratory_RightAlveoli_BaseCompliance"] = &PhysiologyThread::GetRightAlveoliBaselineCompliance;
        nodePathTable["Respiratory_CarbonDioxide_Exhaled"] = &PhysiologyThread::GetExhaledCO2;

        // Energy system
        nodePathTable["Energy_Core_Temperature"] = &PhysiologyThread::GetCoreTemperature;

        // Blood chemistry system
        nodePathTable["BloodChemistry_WhiteBloodCell_Count"] = &PhysiologyThread::GetWhiteBloodCellCount;
        nodePathTable["BloodChemistry_RedBloodCell_Count"] = &PhysiologyThread::GetRedBloodCellCount;
        nodePathTable["BloodChemistry_BloodUreaNitrogen_Concentration"] = &PhysiologyThread::GetBUN;
        nodePathTable["BloodChemistry_Oxygen_Saturation"] = &PhysiologyThread::GetOxygenSaturation;
        nodePathTable["BloodChemistry_Hemaocrit"] = &PhysiologyThread::GetHematocrit;
        nodePathTable["BloodChemistry_BloodPH"] = &PhysiologyThread::GetBloodPH;
        nodePathTable["BloodChemistry_Arterial_CarbonDioxide_Pressure"] = &PhysiologyThread::GetArterialCarbonDioxidePressure;
        nodePathTable["BloodChemistry_Arterial_Oxygen_Pressure"] = &PhysiologyThread::GetArterialOxygenPressure;
        nodePathTable["BloodChemistry_VenousOxygenPressure"] = &PhysiologyThread::GetVenousOxygenPressure;
        nodePathTable["BloodChemistry_VenousCarbonDioxidePressure"] = &PhysiologyThread::GetVenousCarbonDioxidePressure;

        // Substances
        nodePathTable["Substance_Sodium"] = &PhysiologyThread::GetSodium;
        nodePathTable["Substance_Sodium_Concentration"] = &PhysiologyThread::GetSodiumConcentration;
        nodePathTable["Substance_Bicarbonate"] = &PhysiologyThread::GetBicarbonate;
        nodePathTable["Substance_Bicarbonate_Concentration"] = &PhysiologyThread::GetBicarbonateConcentration;
        nodePathTable["Substance_BaseExcess"] = &PhysiologyThread::GetBaseExcess;
        nodePathTable["Substance_Glucose_Concentration"] = &PhysiologyThread::GetGlucoseConcentration;
        nodePathTable["Substance_Creatinine_Concentration"] = &PhysiologyThread::GetCreatinineConcentration;
        nodePathTable["Substance_Hemoglobin_Concentration"] = &PhysiologyThread::GetHemoglobinConcentration;
        nodePathTable["Substance_Calcium_Concentration"] = &PhysiologyThread::GetCalciumConcentration;
        nodePathTable["Substance_Albumin_Concentration"] = &PhysiologyThread::GetAlbuminConcentration;

        nodePathTable["MetabolicPanel_Bilirubin"] = &PhysiologyThread::GetTotalBilirubin;
        nodePathTable["MetabolicPanel_Protein"] = &PhysiologyThread::GetTotalProtein;
        nodePathTable["MetabolicPanel_CarbonDioxide"] = &PhysiologyThread::GetCO2;
        nodePathTable["MetabolicPanel_Potassium"] = &PhysiologyThread::GetPotassium;
        nodePathTable["MetabolicPanel_Chloride"] = &PhysiologyThread::GetChloride;

        nodePathTable["CompleteBloodCount_Platelet"] = &PhysiologyThread::GetPlateletCount;

        // Label which nodes are high-frequency
        highFrequencyNodes = {
                "ECG",
                "Cardiovascular_HeartRate",
                "Cardiovascular_Arterial_Pressure",
                "Respiratory_CarbonDioxide_Exhaled",
                "Respiratory_Respiration_Rate"
        };

    }


    std::map<std::string, double (PhysiologyThread::*)()> *PhysiologyThread::GetNodePathTable() {
        return &nodePathTable;
    }

    void PhysiologyThread::Shutdown() {

    }

    void PhysiologyThread::StartSimulation() {
        if (!m_runThread) {
            m_runThread = true;
            //     m_thread = std::thread(&PhysiologyThread::AdvanceTime, this);
        }
    }

    void PhysiologyThread::StopSimulation() {
        if (m_runThread) {
            m_runThread = false;
            //   m_thread.join();
        }
    }

    std::string PhysiologyThread::getTimestampedFilename(const std::string &basePathname) {
        std::ostringstream filename;
        filename << basePathname << static_cast<unsigned long>(::time(0)) << ".csv";
        return filename.str();
    }

    bool PhysiologyThread::LoadState(const std::string &stateFile, double sec) {
        SEScalarTime startTime;
        startTime.SetValue(sec, TimeUnit::s);

        LOG_INFO << "Loading state file " << stateFile << " at position " << sec << " seconds";
        if (!m_pe->LoadState(stateFile, &startTime)) {
            LOG_ERROR << "Error initializing state";
            return false;
        }

        PreloadSubstances();
        PreloadCompartments();

        if (logging_enabled) {
            std::string logFilename = getTimestampedFilename("./logs/Output_");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set("HeartRate",
                                                                                              FrequencyUnit::Per_min);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set("MeanArterialPressure",
                                                                                              PressureUnit::mmHg);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "SystolicArterialPressure",
                    PressureUnit::mmHg);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "DiastolicArterialPressure",
                    PressureUnit::mmHg);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set("RespirationRate",
                                                                                              FrequencyUnit::Per_min);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set("TidalVolume",
                                                                                              VolumeUnit::mL);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set("TotalLungVolume",
                                                                                              VolumeUnit::mL);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set("OxygenSaturation");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateLiquidCompartmentDataRequest().Set(
                    BGE::VascularCompartment::Aorta, *O2, "PartialPressure");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateLiquidCompartmentDataRequest().Set(
                    BGE::VascularCompartment::Aorta, *CO2, "PartialPressure");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::Lungs, "Volume");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::Carina, "InFlow");
            m_pe->GetEngineTrack()->GetDataRequestManager().SetResultsFilename(logFilename);
        }
        return true;
    }

    bool PhysiologyThread::SaveState() {
        m_pe->SaveState();
        return true;
    }

    bool PhysiologyThread::SaveState(const std::string &stateFile) {
        m_pe->SaveState(stateFile);
        return true;
    }

    bool PhysiologyThread::ExecuteCommand(const std::string &cmd) {
        string scenarioFile = "Actions/" + cmd + ".xml";
        return LoadScenarioFile(scenarioFile);
    }

// Load a scenario from an XML file, apply conditions and iterate through the actions
// This bypasses the standard BioGears ExecuteScenario method to avoid resetting the BioGears engine
    bool PhysiologyThread::LoadScenarioFile(const std::string &scenarioFile) {
        SEScenario sce(m_pe->GetSubstanceManager());
        sce.LoadFile(scenarioFile);

        double dT_s = m_pe->GetTimeStep(TimeUnit::s);
        // double scenarioTime_s;

        double sampleTime_s = sce.GetDataRequestManager().GetSamplesPerSecond();
        if (sampleTime_s != 0)
            sampleTime_s = 1 / sampleTime_s;
        double currentSampleTime_s = sampleTime_s;        //Sample the first step

        SEAdvanceTime *adv;
        for (SEAction *a : sce.GetActions()) {
            adv = dynamic_cast<SEAdvanceTime *>(a);
            if (adv != nullptr) {
                double time_s = adv->GetTime(TimeUnit::s);
                auto count = (int) (time_s / dT_s);
                for (int i = 0; i <= count; i++) {
                    m_pe->AdvanceModelTime();
                    currentSampleTime_s += dT_s;
                    if (currentSampleTime_s >= sampleTime_s) {
                        currentSampleTime_s = 0;
                    }
                }
                continue;
            } else {
                m_pe->ProcessAction(*a);
            }
        }
        return true;
    }

    void PhysiologyThread::AdvanceTimeTick() {
        m_mutex.lock();
        m_pe->AdvanceModelTime();
        if (logging_enabled) {
            m_pe->GetEngineTrack()->TrackData(m_pe->GetSimulationTime(TimeUnit::s));
        }
        m_mutex.unlock();
    }

    double PhysiologyThread::GetShutdownMessage() {
        return -1;
    }

    double PhysiologyThread::GetSimulationTime() {
        return m_pe->GetSimulationTime(TimeUnit::s);
    }

    double PhysiologyThread::GetNodePath(const std::string &nodePath) {
        std::map<std::string, double (PhysiologyThread::*)()>::iterator entry;
        entry = nodePathTable.find(nodePath);
        if (entry != nodePathTable.end()) {
            return (this->*(entry->second))();
        }

        LOG_ERROR << "Unable to access nodepath " << nodePath;
        return 0;

    }

    double PhysiologyThread::GetHeartRate() {
        return m_pe->GetCardiovascularSystem()->GetHeartRate(FrequencyUnit::Per_min);
    }

// ?? - Blood Volume - mL
    double PhysiologyThread::GetBloodVolume() {
        return m_pe->GetCardiovascularSystem()->GetBloodVolume(VolumeUnit::mL);
    }

// SYS (ART) - Arterial Systolic Pressure - mmHg
    double PhysiologyThread::GetArterialSystolicPressure() {
        return m_pe->GetCardiovascularSystem()->GetSystolicArterialPressure(PressureUnit::mmHg);
    }

// DIA (ART) - Arterial Diastolic Pressure - mmHg
    double PhysiologyThread::GetArterialDiastolicPressure() {
        return m_pe->GetCardiovascularSystem()->GetDiastolicArterialPressure(PressureUnit::mmHg);
    }

// MAP (ART) - Mean Arterial Pressure - mmHg
    double PhysiologyThread::GetMeanArterialPressure() {
        return m_pe->GetCardiovascularSystem()->GetMeanArterialPressure(PressureUnit::mmHg);
    }

// AP - Arterial Pressure - mmHg
    double PhysiologyThread::GetArterialPressure() {
        return m_pe->GetCardiovascularSystem()->GetArterialPressure(PressureUnit::mmHg);
    }

// CVP - Central Venous Pressure - mmHg
    double PhysiologyThread::GetMeanCentralVenousPressure() {
        return m_pe->GetCardiovascularSystem()->GetMeanCentralVenousPressure(PressureUnit::mmHg);
    }

// MCO2 - End Tidal Carbon Dioxide Fraction - unitless %
    double PhysiologyThread::GetEndTidalCarbonDioxideFraction() {
        return (m_pe->GetRespiratorySystem()->GetEndTidalCarbonDioxideFraction() * 7.5);
    }

// SPO2 - Oxygen Saturation - unitless %
    double PhysiologyThread::GetOxygenSaturation() {
        return m_pe->GetBloodChemistrySystem()->GetOxygenSaturation() * 100;
    }

// BR - Respiration Rate - per minute
    double PhysiologyThread::GetRespirationRate() {
        return m_pe->GetRespiratorySystem()->GetRespirationRate(FrequencyUnit::Per_min);
    }

// T2 - Core Temperature - degrees C
    double PhysiologyThread::GetCoreTemperature() {
        return m_pe->GetEnergySystem()->GetCoreTemperature(TemperatureUnit::C);
    }

// ECG Waveform in mV
    double PhysiologyThread::GetECGWaveform() {
        double ecgLead3_mV = m_pe->GetElectroCardioGram()->GetLead3ElectricPotential(ElectricPotentialUnit::mV);
        return ecgLead3_mV;
    }

// Na+ - Sodium Concentration - mg/dL
    double PhysiologyThread::GetSodiumConcentration() {
        return sodium->GetBloodConcentration(MassPerVolumeUnit::mg_Per_dL);
    }

// Na+ - Sodium - mmol/L
    double PhysiologyThread::GetSodium() {
        return GetSodiumConcentration() * 0.43;
    }

// Glucose - Glucose Concentration - mg/dL
    double PhysiologyThread::GetGlucoseConcentration() {
        return glucose->GetBloodConcentration(MassPerVolumeUnit::mg_Per_dL);
    }

// BUN - BloodUreaNitrogenConcentration - mg/dL
    double PhysiologyThread::GetBUN() {
        return m_pe->GetBloodChemistrySystem()->GetBloodUreaNitrogenConcentration(MassPerVolumeUnit::mg_Per_dL);
    }

// Creatinine - Creatinine Concentration - mg/dL
    double PhysiologyThread::GetCreatinineConcentration() {
        return creatinine->GetBloodConcentration(MassPerVolumeUnit::mg_Per_dL);
    }

    double PhysiologyThread::GetCalciumConcentration() {
        return calcium->GetBloodConcentration(MassPerVolumeUnit::mg_Per_dL);
    }

    double PhysiologyThread::GetAlbuminConcentration() {
        return albumin->GetBloodConcentration(MassPerVolumeUnit::mg_Per_dL);
    }

    double PhysiologyThread::GetTotalBilirubin() {
        return m_pe->GetBloodChemistrySystem()->GetTotalBilirubin(MassPerVolumeUnit::mg_Per_dL);
    }

    double PhysiologyThread::GetTotalProtein() {
        SEComprehensiveMetabolicPanel metabolicPanel(GetLogger());
        m_pe->GetPatientAssessment(metabolicPanel);
        SEScalarMassPerVolume protein = metabolicPanel.GetTotalProtein();
        return protein.GetValue(MassPerVolumeUnit::mg_Per_dL);
    }

// RBC - White Blood Cell Count - ct/uL
    double PhysiologyThread::GetWhiteBloodCellCount() {
        return m_pe->GetBloodChemistrySystem()->GetWhiteBloodCellCount(AmountPerVolumeUnit::ct_Per_uL) / 1000;
    }

// RBC - Red Blood Cell Count - ct/uL
    double PhysiologyThread::GetRedBloodCellCount() {
        return m_pe->GetBloodChemistrySystem()->GetRedBloodCellCount(AmountPerVolumeUnit::ct_Per_uL) / 1000000;
    }

// Hgb - Hemoglobin Concentration - g/dL
    double PhysiologyThread::GetHemoglobinConcentration() {
        return hemoglobin->GetBloodConcentration(MassPerVolumeUnit::g_Per_dL);
    }

// Hct - Hematocrit - unitless
    double PhysiologyThread::GetHematocrit() {
        return m_pe->GetBloodChemistrySystem()->GetHematocrit() * 100;
    }

// pH - Blood pH - unitless
    double PhysiologyThread::GetBloodPH() {
        return m_pe->GetBloodChemistrySystem()->GetBloodPH();
    }

// PaCO2 - Arterial Carbon Dioxide Pressure - mmHg
    double PhysiologyThread::GetArterialCarbonDioxidePressure() {
        return m_pe->GetBloodChemistrySystem()->GetArterialCarbonDioxidePressure(PressureUnit::mmHg);
    }

// Pa02 - Arterial Oxygen Pressure - mmHg
    double PhysiologyThread::GetArterialOxygenPressure() {
        return m_pe->GetBloodChemistrySystem()->GetArterialOxygenPressure(PressureUnit::mmHg);
    }

    double PhysiologyThread::GetVenousOxygenPressure() {
        return m_pe->GetBloodChemistrySystem()->GetVenousOxygenPressure(PressureUnit::mmHg);
    }

    double PhysiologyThread::GetVenousCarbonDioxidePressure() {
        return m_pe->GetBloodChemistrySystem()->GetVenousCarbonDioxidePressure(PressureUnit::mmHg);
    }

// n/a - Bicarbonate Concentration - mg/L
    double PhysiologyThread::GetBicarbonateConcentration() {
        return bicarbonate->GetBloodConcentration(MassPerVolumeUnit::mg_Per_dL);
    }

// HCO3 - Bicarbonate - Convert to mmol/L
    double PhysiologyThread::GetBicarbonate() {
        return GetBicarbonateConcentration() * 0.1639;
    }

// BE - Base Excess -
    double PhysiologyThread::GetBaseExcess() {
        return (0.93 * GetBicarbonate()) + (13.77 * GetBloodPH()) - 124.58;
    }

    double PhysiologyThread::GetCO2() {
        SEComprehensiveMetabolicPanel metabolicPanel(GetLogger());
        m_pe->GetPatientAssessment(metabolicPanel);
        SEScalarAmountPerVolume CO2 = metabolicPanel.GetCO2();
        return CO2.GetValue(AmountPerVolumeUnit::mmol_Per_L);
    }

    double PhysiologyThread::GetPotassium() {
        SEComprehensiveMetabolicPanel metabolicPanel(GetLogger());
        m_pe->GetPatientAssessment(metabolicPanel);
        SEScalarAmountPerVolume potassium = metabolicPanel.GetPotassium();
        return potassium.GetValue(AmountPerVolumeUnit::ct_Per_uL);
    }

    double PhysiologyThread::GetChloride() {
        SEComprehensiveMetabolicPanel metabolicPanel(GetLogger());
        m_pe->GetPatientAssessment(metabolicPanel);
        SEScalarAmountPerVolume chloride = metabolicPanel.GetChloride();
        return chloride.GetValue(AmountPerVolumeUnit::ct_Per_uL);
    }

// PLT - Platelet Count - ct/uL
    double PhysiologyThread::GetPlateletCount() {
        SECompleteBloodCount CBC(GetLogger());
        m_pe->GetPatientAssessment(CBC);
        SEScalarAmountPerVolume plateletCount = CBC.GetPlateletCount();
        return plateletCount.GetValue(AmountPerVolumeUnit::ct_Per_uL) / 1000;
    }

// GetExhaledCO2 - tracheal CO2 partial pressure - mmHg
    double PhysiologyThread::GetExhaledCO2() {
        return carina->GetSubstanceQuantity(*CO2)->GetPartialPressure(PressureUnit::mmHg);
    }

// Get Tidal Volume - mL
    double PhysiologyThread::GetTidalVolume() {
        return m_pe->GetRespiratorySystem()->GetTidalVolume(VolumeUnit::mL);
    }

// Get Total Lung Volume - mL
    double PhysiologyThread::GetTotalLungVolume() {
        return m_pe->GetRespiratorySystem()->GetTotalLungVolume(VolumeUnit::mL);
    }

// Get Left Lung Volume - mL
    double PhysiologyThread::GetLeftLungVolume() {
        return leftLung->GetVolume(VolumeUnit::mL);
    }

// Get Right Lung Volume - mL
    double PhysiologyThread::GetRightLungVolume() {
        return rightLung->GetVolume(VolumeUnit::mL);
    }

// Get Left Lung Pleural Cavity Volume - mL
    double PhysiologyThread::GetLeftPleuralCavityVolume() {
        return leftLung->GetVolume(VolumeUnit::mL);
    }

// Get Right Lung Pleural Cavity Volume - mL
    double PhysiologyThread::GetRightPleuralCavityVolume() {
        return rightLung->GetVolume(VolumeUnit::mL);
    }

// Get left alveoli baseline compliance (?) volume
    double PhysiologyThread::GetLeftAlveoliBaselineCompliance() {
        return leftLung->GetVolume(VolumeUnit::mL);
    }

// Get right alveoli baseline compliance (?) volume
    double PhysiologyThread::GetRightAlveoliBaselineCompliance() {
        return rightLung->GetVolume(VolumeUnit::mL);
    }

    double PhysiologyThread::GetCardiacOutput() {
        return m_pe->GetCardiovascularSystem()->GetCardiacOutput(VolumePerTimeUnit::mL_Per_min);
    }

    void PhysiologyThread::SetVentilator(const std::string &ventilatorSettings) {
        vector<string> strings = explode("\n", ventilatorSettings);

        SEAnesthesiaMachineConfiguration AMConfig(m_pe->GetSubstanceManager());
        SEAnesthesiaMachine &config = AMConfig.GetConfiguration();

	/**
	   LOG_TRACE << "Setting base values";
	   config.SetConnection(CDM::enumAnesthesiaMachineConnection::Mask);
	   config.GetInletFlow().SetValue(2.0, VolumePerTimeUnit::L_Per_min);
	   config.GetInspiratoryExpiratoryRatio().SetValue(.5);
	   config.SetOxygenSource(CDM::enumAnesthesiaMachineOxygenSource::Wall);
	   config.SetPrimaryGas(CDM::enumAnesthesiaMachinePrimaryGas::Nitrogen);
	   config.GetReliefValvePressure().SetValue(20.0, PressureUnit::cmH2O);
	   config.GetVentilatorPressure().SetValue(0.0, PressureUnit::cmH2O);
	   config.GetOxygenBottleOne().GetVolume().SetValue(660.0, VolumeUnit::L);
	   config.GetOxygenBottleTwo().GetVolume().SetValue(660.0, VolumeUnit::L);
	**/
	
        for (auto str : strings) {
	  vector<string> strs;
	  boost::split(strs, str, boost::is_any_of("="));
	  auto strs_size = strs.size();
	  // Check if it's not a key value pair
	  if (strs_size != 2) {
	    continue;
	  }
	  std::string kvp_k = strs[0];
	  double kvp_v = std::stod(strs[1]);
	  try {
	      if (kvp_k == "OxygenFraction") {
                config.GetOxygenFraction().SetValue(kvp_v);
	      } else if (kvp_k == "PositiveEndExpiredPressure") {
                config.GetPositiveEndExpiredPressure().SetValue(kvp_v, PressureUnit::cmH2O);
	      } else if (kvp_k == "RespiratoryRate") {
                config.GetRespiratoryRate().SetValue(kvp_v, FrequencyUnit::Per_min);
	      } else if (kvp_k == "InspiratoryExpiratoryRatio") {
		config.GetInspiratoryExpiratoryRatio().SetValue(kvp_v);
	      } else if (kvp_k == "TidalVolume") {
		// empty
	      } else if (kvp_k == "VentilatorPressure") {
		config.GetVentilatorPressure().SetValue(kvp_v, PressureUnit::cmH2O);
	      } else if (kvp_k == " ") {
		// empty
	      } else {
                LOG_INFO << "Unknown ventilator setting: " << kvp_k << " = " << kvp_v;
	      }
	  } catch (exception &e) {
	    LOG_ERROR << "Issue with setting " << e.what();
	  }
        }
	
	try {
	  m_pe->ProcessAction(AMConfig);
	} catch (exception &e) {
	  LOG_ERROR << "Error processing ventilator action: " << e.what();
	}
    }

    void PhysiologyThread::Status() {
        GetLogger()->Info("");
        GetLogger()->Info(
                std::stringstream() << "Simulation Time : " << m_pe->GetSimulationTime(TimeUnit::s) << "s");
        GetLogger()->Info(
                std::stringstream() << "Cardiac Output : "
                                    << m_pe->GetCardiovascularSystem()->GetCardiacOutput(VolumePerTimeUnit::mL_Per_min)
                                    << VolumePerTimeUnit::mL_Per_min);
        GetLogger()->Info(
                std::stringstream() << "Blood Volume : "
                                    << m_pe->GetCardiovascularSystem()->GetBloodVolume(VolumeUnit::mL)
                                    << VolumeUnit::mL);
        GetLogger()->Info(
                std::stringstream() << "Mean Arterial Pressure : "
                                    << m_pe->GetCardiovascularSystem()->GetMeanArterialPressure(PressureUnit::mmHg)
                                    << PressureUnit::mmHg);
        GetLogger()->Info(
                std::stringstream() << "Systolic Pressure : "
                                    << m_pe->GetCardiovascularSystem()->GetSystolicArterialPressure(PressureUnit::mmHg)
                                    << PressureUnit::mmHg);
        GetLogger()->Info(
                std::stringstream() << "Diastolic Pressure : "
                                    << m_pe->GetCardiovascularSystem()->GetDiastolicArterialPressure(PressureUnit::mmHg)
                                    << PressureUnit::mmHg);
        GetLogger()->Info(
                std::stringstream() << "Heart Rate : "
                                    << m_pe->GetCardiovascularSystem()->GetHeartRate(FrequencyUnit::Per_min)
                                    << "bpm");
        GetLogger()->Info(
                std::stringstream() << "Respiration Rate : "
                                    << m_pe->GetRespiratorySystem()->GetRespirationRate(FrequencyUnit::Per_min)
                                    << "bpm");
        GetLogger()->Info("");
    }
}
