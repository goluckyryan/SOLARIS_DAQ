#ifndef  DIGITIZER_PARAMETER_H
#define  DIGITIZER_PARAMETER_H

#include <cstdlib>
#include <vector>

enum RW { ReadOnly, WriteOnly, ReadWrite};

//^==================== Register Class
class Reg {
  private:
    std::string name;
    RW readWrite; // true for read/write, false for read-only
    
  public:
    Reg(std::string para, RW readwrite){
      this->name = para;
      this->readWrite = readwrite;
    }
    ~Reg(){};

    std::string GetPara()   const {return name;}
    RW          ReadWrite() const {return readWrite;}

    operator std::string () const {return name;} // this allow Reg kaka("XYZ", true); std::string haha = kaka;

};


//^==================== Some digitizer parameters

// To avoid typo

namespace DIGIPARA{

  const unsigned short TraceStep = 8;

  namespace DIG{
    
    ///============== read only
    const Reg CupVer                   ("CupVer", RW::ReadOnly);
    const Reg FPGA_firmwareVersion     ("FPGA_FwVer", RW::ReadOnly);
    const Reg FirmwareType             ("FwType", RW::ReadOnly);
    const Reg ModelCode                ("ModelCode", RW::ReadOnly);
    const Reg PBCode                   ("PBCode", RW::ReadOnly);
    const Reg ModelName                ("ModelName", RW::ReadOnly);
    const Reg FromFactor               ("FormFactor", RW::ReadOnly);
    const Reg FamilyCode               ("FamilyCode", RW::ReadOnly);
    const Reg SerialNumber             ("SerialNum", RW::ReadOnly);
    const Reg PCBrev_MB                ("PCBrev_MB", RW::ReadOnly);
    const Reg PCBrev_PB                ("PCBrev_PB", RW::ReadOnly);
    const Reg DPP_License              ("License", RW::ReadOnly);
    const Reg DPP_LicenseStatus        ("LicenseStatus", RW::ReadOnly);
    const Reg DPP_LicenseRemainingTime ("LicenseRemainingTime", RW::ReadOnly);
    const Reg NumberOfChannel          ("NumCh", RW::ReadOnly);
    const Reg ADC_SampleRate           ("ADC_SamplRate", RW::ReadOnly);
    const Reg InputDynamicRange        ("InputRange", RW::ReadOnly);
    const Reg InputType                ("InputType", RW::ReadOnly);
    const Reg InputImpedance           ("Zin", RW::ReadOnly);
    const Reg IPAddress                ("IPAddress", RW::ReadOnly);
    const Reg NetMask                  ("Netmask", RW::ReadOnly);
    const Reg Gateway                  ("Gateway", RW::ReadOnly);
    const Reg LED_status               ("LedStatus", RW::ReadOnly);
    const Reg ACQ_status               ("AcquistionStatus", RW::ReadOnly);
    const Reg MaxRawDataSize           ("MaxRawDataSize", RW::ReadOnly);
    const Reg TempSensAirIn            ("TempSensAirIn", RW::ReadOnly);
    const Reg TempSensAirOut           ("TempSensAirOut", RW::ReadOnly);
    const Reg TempSensCore             ("TempSensCore", RW::ReadOnly);
    const Reg TempSensFirstADC         ("TempSensFirstADC", RW::ReadOnly);
    const Reg TempSensLastADC          ("TempSensLastADC", RW::ReadOnly);
    const Reg TempSensHottestADC       ("TempSensHottestADC", RW::ReadOnly);
    const Reg TempSensADC0             ("TempSensADC0", RW::ReadOnly);
    const Reg TempSensADC1             ("TempSensADC1", RW::ReadOnly);
    const Reg TempSensADC2             ("TempSensADC2", RW::ReadOnly);
    const Reg TempSensADC3             ("TempSensADC3", RW::ReadOnly);
    const Reg TempSensADC4             ("TempSensADC4", RW::ReadOnly);
    const Reg TempSensADC5             ("TempSensADC5", RW::ReadOnly);
    const Reg TempSensADC6             ("TempSensADC6", RW::ReadOnly);
    const Reg TempSensADC7             ("TempSensADC7", RW::ReadOnly);

    const std::vector<Reg> TempSenseADC = {TempSensADC0,TempSensADC1,TempSensADC2,TempSensADC3,TempSensADC4,TempSensADC5,TempSensADC6,TempSensADC7};

    const Reg TempSensDCDC             ("TempSensDCDC", RW::ReadOnly);
    const Reg VInSensDCDC              ("VInSensDCDC", RW::ReadOnly);
    const Reg VOutSensDCDC             ("VOutSensDCDC", RW::ReadOnly);
    const Reg IOutSensDCDC             ("IOutSensDCDC", RW::ReadOnly);
    const Reg FreqSensCore             ("FreqSensCore", RW::ReadOnly);
    const Reg DutyCycleSensDCDC        ("DutyCycleSensDCDC", RW::ReadOnly);
    const Reg SpeedSensFan1            ("SpeedSensFan1", RW::ReadOnly);
    const Reg SpeedSensFan2            ("SpeedSensFan2", RW::ReadOnly);
    const Reg ErrorFlags               ("ErrorFlags", RW::ReadOnly);
    const Reg BoardReady               ("BoardReady", RW::ReadOnly);
  
    ///============= read write
    const Reg ClockSource              ("ClockSource", RW::ReadWrite);
    const Reg IO_Level                 ("IOlevel", RW::ReadWrite);
    const Reg StartSource              ("StartSource", RW::ReadWrite);
    const Reg GlobalTriggerSource      ("GlobalTriggerSource", RW::ReadWrite);

    const Reg BusyInSource             ("BusyInSource", RW::ReadWrite);
    const Reg EnableClockOutBackplane  ("EnClockOutP0", RW::ReadWrite);
    const Reg EnableClockOutFrontPanel ("EnClockOutFP", RW::ReadWrite);
    const Reg TrgOutMode               ("TrgOutMode", RW::ReadWrite);
    const Reg GPIOMode                 ("GPIOMode", RW::ReadWrite);
    const Reg SyncOutMode              ("SyncOutMode", RW::ReadWrite);

    const Reg BoardVetoSource          ("BoardVetoSource", RW::ReadWrite);
    const Reg BoardVetoWidth           ("BoardVetoWidth", RW::ReadWrite);
    const Reg BoardVetoPolarity        ("BoardVetoPolarity", RW::ReadWrite);
    const Reg RunDelay                 ("RunDelay", RW::ReadWrite);
    const Reg EnableAutoDisarmACQ      ("EnAutoDisarmAcq", RW::ReadWrite);
    const Reg EnableDataReduction      ("EnDataReduction", RW::ReadWrite);
    const Reg EnableStatisticEvents    ("EnStatEvents", RW::ReadWrite);
    const Reg VolatileClockOutDelay    ("VolatileClockOutDelay", RW::ReadWrite);
    const Reg PermanentClockOutDelay   ("PermanentClockOutDelay", RW::ReadWrite);
    const Reg TestPulsePeriod          ("TestPulsePeriod", RW::ReadWrite);
    const Reg TestPulseWidth           ("TestPulseWidth", RW::ReadWrite);
    const Reg TestPulseLowLevel        ("TestPulseLowLevel", RW::ReadWrite);
    const Reg TestPulseHighLevel       ("TestPulseHighLevel", RW::ReadWrite);
    const Reg ErrorFlagMask            ("ErrorFlagMask", RW::ReadWrite);
    const Reg ErrorFlagDataMask        ("ErrorFlagDataMask", RW::ReadWrite);
    const Reg DACoutMode               ("DACoutMode", RW::ReadWrite);
    const Reg DACoutStaticLevel        ("DACoutStaticLevel", RW::ReadWrite);
    const Reg DACoutChSelect           ("DACoutChSelect", RW::ReadWrite);
    const Reg EnableOffsetCalibration  ("EnOffsetCalibration", RW::ReadWrite);

    /// ========== command
    const Reg Reset               ("Reset", RW::WriteOnly);
    const Reg ClearData           ("ClearData", RW::WriteOnly); // clear memory, setting not affected
    const Reg ArmACQ              ("ArmAcquisition", RW::WriteOnly);
    const Reg DisarmACQ           ("DisarmAcquisition", RW::WriteOnly);
    const Reg SoftwareStartACQ    ("SwStartAcquisition", RW::WriteOnly); // only when SwStart in StartSource
    const Reg SoftwareStopACQ     ("SwStopAcquisition", RW::WriteOnly); // stop ACQ, whatever start source
    const Reg SendSoftwareTrigger ("SendSWTrigger", RW::WriteOnly); // only work when Swtrg in the GlobalTriggerSource
    const Reg ReloadCalibration   ("ReloadCalibration", RW::WriteOnly); 


    const std::vector<Reg> AllSettings = {
      CupVer                   ,
      FPGA_firmwareVersion     ,
      FirmwareType             ,
      ModelCode                ,
      PBCode                   ,
      ModelName                ,
      FromFactor               ,
      FamilyCode               ,
      SerialNumber             ,
      PCBrev_MB                ,
      PCBrev_PB                ,
      DPP_License              ,
      DPP_LicenseStatus        ,
      DPP_LicenseRemainingTime ,
      NumberOfChannel          ,
      ADC_SampleRate           ,
      InputDynamicRange        ,
      InputType                ,
      InputImpedance           ,
      IPAddress                ,
      NetMask                  ,
      Gateway                  ,
      LED_status               ,
      ACQ_status               ,
      MaxRawDataSize           ,
      TempSensAirIn            ,
      TempSensAirOut           ,
      TempSensCore             ,
      TempSensFirstADC         ,
      TempSensLastADC          ,
      TempSensHottestADC       ,
      TempSensADC0             ,
      TempSensADC1             ,
      TempSensADC2             ,
      TempSensADC3             ,
      TempSensADC4             ,
      TempSensADC5             ,
      TempSensADC6             ,
      TempSensADC7             ,
      TempSensDCDC             ,
      VInSensDCDC              ,
      VOutSensDCDC             ,
      IOutSensDCDC             ,
      FreqSensCore             ,
      DutyCycleSensDCDC        ,
      SpeedSensFan1            ,
      SpeedSensFan2            ,
      ErrorFlags               ,
      BoardReady               ,
      ClockSource              ,
      IO_Level                 ,
      StartSource              ,
      GlobalTriggerSource      ,
      BusyInSource             ,
      EnableClockOutBackplane  ,
      EnableClockOutFrontPanel ,
      TrgOutMode               ,
      GPIOMode                 ,
      SyncOutMode              ,
      BoardVetoSource          ,
      BoardVetoWidth           ,
      BoardVetoPolarity        ,
      RunDelay                 ,
      EnableAutoDisarmACQ      ,
      EnableDataReduction      ,
      EnableStatisticEvents    ,
      VolatileClockOutDelay    ,
      PermanentClockOutDelay   ,
      TestPulsePeriod          ,
      TestPulseWidth           ,
      TestPulseLowLevel        ,
      TestPulseHighLevel       ,
      ErrorFlagMask            ,
      ErrorFlagDataMask        ,
      DACoutMode               ,
      DACoutStaticLevel        ,
      DACoutChSelect           ,
      EnableOffsetCalibration  
    };


  }

  namespace VGA{
    const Reg VGAGain ("VGAGain", RW::ReadWrite); // VX2745 only
  }

  namespace CH{

    /// ========= red only
    const Reg SelfTrgRate          ("SelfTrgRate", RW::ReadOnly);
    const Reg ChannelStatus        ("ChStatus", RW::ReadOnly);
    const Reg GainFactor           ("GainFactor", RW::ReadOnly);
    const Reg ADCToVolts           ("ADCToVolts", RW::ReadOnly);
    const Reg Energy_Nbit          ("Energy_Nbit", RW::ReadOnly);
    const Reg ChannelRealtime      ("ChRealtimeMonitor", RW::ReadOnly); // when called, update DeadTime, TriggerCount, SaveCount, and WaveCount
    const Reg ChannelDeadtime      ("ChDeadtimeMonitor", RW::ReadOnly);
    const Reg ChannelTriggerCount  ("ChTriggerCnt", RW::ReadOnly);
    const Reg ChannelSavedCount    ("ChSavedEventCnt", RW::ReadOnly);
    const Reg ChannelWaveCount     ("ChWaveCnt", RW::ReadOnly);

    /// ======= read write
    const Reg ChannelEnable    ("ChEnable", RW::ReadWrite);
    const Reg DC_Offset        ("DCOffset", RW::ReadWrite);
    const Reg TriggerThreshold ("TriggerThr", RW::ReadWrite);
    const Reg Polarity         ("Pulse Polarity", RW::ReadWrite);

    const Reg WaveDataSource              ("WaveDataSouce", RW::ReadWrite);
    const Reg RecordLength                ("ChRecordLengthT", RW::ReadWrite);
    const Reg PreTrigger                  ("ChPreTriggerT", RW::ReadWrite);
    const Reg WaveSaving                  ("WaveSaving", RW::ReadWrite);
    const Reg WaveResolution              ("WaveResolution", RW::ReadWrite);
    const Reg TimeFilterRiseTime          ("TimeFilterRiseTimeT", RW::ReadWrite);
    const Reg TimeFilterRetriggerGuard    ("TimeFilterRetriggerGuardT", RW::ReadWrite);
    const Reg EnergyFilterRiseTime        ("EnergyFilterRiseTimeT", RW::ReadWrite);
    const Reg EnergyFilterFlatTop         ("EnergyFilterFlatTopT", RW::ReadWrite);
    const Reg EnergyFilterPoleZero        ("EnergyFilterPoleZeroT", RW::ReadWrite);
    const Reg EnergyFilterBaselineGuard   ("EnergyFilterBaselineGuardT", RW::ReadWrite);
    const Reg EnergyFilterPileUpGuard     ("EnergyFilterPileUpGuardT", RW::ReadWrite);
    const Reg EnergyFilterPeakingPosition ("EnergyFilterPeakingPosition", RW::ReadWrite);
    const Reg EnergyFilterPeakingAvg      ("EnergyFilterPeakingAvg", RW::ReadWrite);
    const Reg EnergyFilterFineGain        ("EnergyFilterFineGain", RW::ReadWrite);
    const Reg EnergyFilterLowFreqFilter   ("EnergyFilterLFLimitation", RW::ReadWrite);
    const Reg EnergyFilterBaselineAvg     ("EnergyFilterBaselineAvg", RW::ReadWrite);
    const Reg WaveAnalogProbe0            ("WaveAnalogProbe0", RW::ReadWrite);
    const Reg WaveAnalogProbe1            ("WaveAnalogProbe1", RW::ReadWrite);
    const Reg WaveDigitalProbe0           ("WaveDigitalProbe0", RW::ReadWrite);
    const Reg WaveDigitalProbe1           ("WaveDigitalProbe1", RW::ReadWrite);
    const Reg WaveDigitalProbe2           ("WaveDigitalProbe2", RW::ReadWrite);
    const Reg WaveDigitalProbe3           ("WaveDigitalProbe3", RW::ReadWrite);

    const std::vector<Reg> AnalogProbe  = {WaveAnalogProbe0, WaveAnalogProbe1};
    const std::vector<Reg> DigitalProbe = {WaveDigitalProbe0, WaveDigitalProbe1, WaveDigitalProbe2, WaveDigitalProbe3};

    const Reg EventTriggerSource      ("EventTriggerSource", RW::ReadWrite);
    const Reg ChannelsTriggerMask     ("ChannelsTriggerMask", RW::ReadWrite);
    const Reg ChannelVetoSource       ("ChannelVetoSource", RW::ReadWrite);
    const Reg WaveTriggerSource       ("WaveTriggerSource", RW::ReadWrite);
    const Reg EventSelector           ("EventSelector", RW::ReadWrite);
    const Reg WaveSelector            ("WaveSelector", RW::ReadWrite);
    const Reg CoincidenceMask         ("CoincidenceMask", RW::ReadWrite);
    const Reg AntiCoincidenceMask     ("AntiCoincidenceMask", RW::ReadWrite);
    const Reg CoincidenceLength       ("CoincidenceLengthT", RW::ReadWrite);
    const Reg CoincidenceLengthSample ("CoincidenceLengthS", RW::ReadWrite);

    const Reg ADCVetoWidth       ("ADCVetoWidth", RW::ReadWrite);

    const Reg EnergySkimLowDiscriminator  ("EnergySkimLowDiscriminator", RW::ReadWrite);
    const Reg EnergySkimHighDiscriminator ("EnergySkimHighDiscriminator", RW::ReadWrite);

    const Reg RecordLengthSample              ("ChRecordLengthS", RW::ReadWrite);
    const Reg PreTriggerSample                ("ChPreTriggerS", RW::ReadWrite);
    const Reg TimeFilterRiseTimeSample        ("TimeFilterRiseTimeS", RW::ReadWrite);
    const Reg TimeFilterRetriggerGuardSample  ("TimeFilterRetriggerGuardS", RW::ReadWrite);
    const Reg EnergyFilterRiseTimeSample      ("EnergyFilterRiseTimeS", RW::ReadWrite);
    const Reg EnergyFilterFlatTopSample       ("EnergyFilterFlatTopS", RW::ReadWrite);
    const Reg EnergyFilterPoleZeroSample      ("EnergyFilterPoleZeroS", RW::ReadWrite);
    const Reg EnergyFilterBaselineGuardSample ("EnergyFilterBaselineGuardS", RW::ReadWrite);
    const Reg EnergyFilterPileUpGuardSample   ("EnergyFilterPileUpGuardS", RW::ReadWrite);

    const std::vector<Reg> AllSettings = {
      SelfTrgRate                ,
      ChannelStatus              ,
      GainFactor                 ,
      ADCToVolts                 ,
      Energy_Nbit                ,
      ChannelRealtime            ,
      ChannelDeadtime            ,
      ChannelTriggerCount        ,
      ChannelSavedCount          ,
      ChannelWaveCount           ,
      ChannelEnable              ,
      DC_Offset                  ,
      TriggerThreshold           ,
      Polarity                   ,
      WaveDataSource             ,
      RecordLength               ,
      WaveSaving                 ,
      WaveResolution             ,
      PreTrigger                 ,
      TimeFilterRiseTime         ,
      TimeFilterRetriggerGuard   ,
      EnergyFilterRiseTime       ,
      EnergyFilterFlatTop        ,
      EnergyFilterPoleZero       ,
      EnergyFilterBaselineGuard  ,
      EnergyFilterPileUpGuard    ,
      EnergyFilterPeakingPosition,
      EnergyFilterPeakingAvg     ,
      EnergyFilterFineGain       ,
      EnergyFilterLowFreqFilter  ,
      EnergyFilterBaselineAvg    ,
      WaveAnalogProbe0           ,
      WaveAnalogProbe1           ,
      WaveDigitalProbe0          ,
      WaveDigitalProbe1          ,
      WaveDigitalProbe2          ,
      WaveDigitalProbe3          ,
      EventTriggerSource         ,
      ChannelsTriggerMask        ,
      ChannelVetoSource          ,
      WaveTriggerSource          ,
      EventSelector              ,
      WaveSelector               ,
      CoincidenceMask            ,
      AntiCoincidenceMask        ,
      CoincidenceLength          ,
      CoincidenceLengthSample    ,
      ADCVetoWidth               ,
      EnergySkimLowDiscriminator ,
      EnergySkimHighDiscriminator,
      RecordLengthSample         ,
      PreTriggerSample           ,
      TimeFilterRiseTimeSample   ,
      TimeFilterRetriggerGuardSample   ,
      EnergyFilterRiseTimeSample       ,
      EnergyFilterFlatTopSample        ,
      EnergyFilterPoleZeroSample       ,
      EnergyFilterBaselineGuardSample  ,
      EnergyFilterPileUpGuardSample    
    };

  }

};


#endif