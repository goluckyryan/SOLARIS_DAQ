#ifndef  DIGITIZER_PARAMETER_H
#define  DIGITIZER_PARAMETER_H

#include <cstdlib>
#include <string>
#include <vector>

enum TYPE {CH, DIG, LVDS, VGA};
enum RW { ReadOnly, WriteOnly, ReadWrite};

//^==================== Register Class
class Reg {
  private:
    std::string name;
    std::string value;
    TYPE type;
    RW readWrite; // true for read/write, false for read-only
    bool isCmd;
    
  public:
    Reg(){
      this->name = "";
      this->readWrite = RW::ReadWrite;
      this->type = TYPE::CH;
      this->isCmd = false;
      this->value = "";
    }
    Reg(std::string para, RW readwrite, TYPE type = TYPE::CH, bool isCmd = false){
      this->name = para;
      this->readWrite = readwrite;
      this->type = type;
      this->isCmd = isCmd;
      this->value = "";
    }
    ~Reg(){};

    void        SetValue(std::string sv) { this->value = sv;}
    std::string GetValue() const { return value;}

    std::string GetPara()   const {return name;}
    std::string GetFullPara(int ch_index = -1) const {
      switch (type){
        case TYPE::DIG:{
            if( isCmd){
              return "/cmd/" + name;
            }else{
              return "/par/" + name;
            }
          }; break;
        case TYPE::CH:{
            std::string haha = "/par/";
            if( isCmd ){
              haha = "/cmd/";
            }
            if( ch_index == -1 ){ 
              return "/ch/0..63" + haha + name;
            }else{
              return "/ch/" + std::to_string(ch_index) + haha + name;
            }
          }; break;
        case TYPE::LVDS:{
            if( ch_index == -1 ){ 
              return "/lvds/0..3/par/" + name;
            }else{
              return "/lvds/" + std::to_string(ch_index) + "/par/"+ name;
            }
          }; break; 
        case TYPE::VGA: {
            if( ch_index == -1 ){ 
              return "/vga/0..3/par/" + name;
            }else{
              return "/vga/" + std::to_string(ch_index) + "/par/"+ name;
            }
          }; break;
        default: 
          return "invalid"; break; 
      }
    }
    RW  ReadWrite() const {return readWrite;}
    TYPE GetType() const {return type;}

    operator std::string () const {return name;} // this allow Reg kaka("XYZ", true); std::string haha = kaka;

};


//^==================== Some digitizer parameters

// To avoid typo

namespace DIGIPARA{

  const unsigned short TraceStep = 8;

  namespace DIG{
    
    ///============== read only
    const Reg CupVer                   ("CupVer", RW::ReadOnly, TYPE::DIG);
    const Reg FPGA_firmwareVersion     ("FPGA_FwVer", RW::ReadOnly, TYPE::DIG);
    const Reg FirmwareType             ("FwType", RW::ReadOnly, TYPE::DIG);
    const Reg ModelCode                ("ModelCode", RW::ReadOnly, TYPE::DIG);
    const Reg PBCode                   ("PBCode", RW::ReadOnly, TYPE::DIG);
    const Reg ModelName                ("ModelName", RW::ReadOnly, TYPE::DIG);
    const Reg FromFactor               ("FormFactor", RW::ReadOnly, TYPE::DIG);
    const Reg FamilyCode               ("FamilyCode", RW::ReadOnly, TYPE::DIG);
    const Reg SerialNumber             ("SerialNum", RW::ReadOnly, TYPE::DIG);
    const Reg PCBrev_MB                ("PCBrev_MB", RW::ReadOnly, TYPE::DIG);
    const Reg PCBrev_PB                ("PCBrev_PB", RW::ReadOnly, TYPE::DIG);
    const Reg DPP_License              ("License", RW::ReadOnly, TYPE::DIG);
    const Reg DPP_LicenseStatus        ("LicenseStatus", RW::ReadOnly, TYPE::DIG);
    const Reg DPP_LicenseRemainingTime ("LicenseRemainingTime", RW::ReadOnly, TYPE::DIG);
    const Reg NumberOfChannel          ("NumCh", RW::ReadOnly, TYPE::DIG);
    const Reg ADC_SampleRate           ("ADC_SamplRate", RW::ReadOnly, TYPE::DIG);
    const Reg InputDynamicRange        ("InputRange", RW::ReadOnly, TYPE::DIG);
    const Reg InputType                ("InputType", RW::ReadOnly, TYPE::DIG);
    const Reg InputImpedance           ("Zin", RW::ReadOnly, TYPE::DIG);
    const Reg IPAddress                ("IPAddress", RW::ReadOnly, TYPE::DIG);
    const Reg NetMask                  ("Netmask", RW::ReadOnly, TYPE::DIG);
    const Reg Gateway                  ("Gateway", RW::ReadOnly, TYPE::DIG);
    const Reg LED_status               ("LedStatus", RW::ReadOnly, TYPE::DIG);
    const Reg ACQ_status               ("AcquisitionStatus", RW::ReadOnly, TYPE::DIG);
    const Reg MaxRawDataSize           ("MaxRawDataSize", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensAirIn            ("TempSensAirIn", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensAirOut           ("TempSensAirOut", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensCore             ("TempSensCore", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensFirstADC         ("TempSensFirstADC", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensLastADC          ("TempSensLastADC", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensHottestADC       ("TempSensHottestADC", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensADC0             ("TempSensADC0", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensADC1             ("TempSensADC1", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensADC2             ("TempSensADC2", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensADC3             ("TempSensADC3", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensADC4             ("TempSensADC4", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensADC5             ("TempSensADC5", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensADC6             ("TempSensADC6", RW::ReadOnly, TYPE::DIG);
    const Reg TempSensADC7             ("TempSensADC7", RW::ReadOnly, TYPE::DIG);

    const std::vector<Reg> TempSensADC = {TempSensADC0,TempSensADC1,TempSensADC2,TempSensADC3,TempSensADC4,TempSensADC5,TempSensADC6,TempSensADC7};

    const Reg TempSensDCDC             ("TempSensDCDC", RW::ReadOnly, TYPE::DIG);
    const Reg VInSensDCDC              ("VInSensDCDC", RW::ReadOnly, TYPE::DIG);
    const Reg VOutSensDCDC             ("VOutSensDCDC", RW::ReadOnly, TYPE::DIG);
    const Reg IOutSensDCDC             ("IOutSensDCDC", RW::ReadOnly, TYPE::DIG);
    const Reg FreqSensCore             ("FreqSensCore", RW::ReadOnly, TYPE::DIG);
    const Reg DutyCycleSensDCDC        ("DutyCycleSensDCDC", RW::ReadOnly, TYPE::DIG);
    const Reg SpeedSensFan1            ("SpeedSensFan1", RW::ReadOnly, TYPE::DIG);
    const Reg SpeedSensFan2            ("SpeedSensFan2", RW::ReadOnly, TYPE::DIG);
    const Reg ErrorFlags               ("ErrorFlags", RW::ReadOnly, TYPE::DIG);
    const Reg BoardReady               ("BoardReady", RW::ReadOnly, TYPE::DIG);
  
    ///============= read write
    const Reg ClockSource              ("ClockSource", RW::ReadWrite, TYPE::DIG);
    const Reg IO_Level                 ("IOlevel", RW::ReadWrite, TYPE::DIG);
    const Reg StartSource              ("StartSource", RW::ReadWrite, TYPE::DIG);
    const Reg GlobalTriggerSource      ("GlobalTriggerSource", RW::ReadWrite, TYPE::DIG);

    const Reg BusyInSource             ("BusyInSource", RW::ReadWrite, TYPE::DIG);
    //const Reg EnableClockOutBackplane  ("EnClockOutP0", RW::ReadWrite, TYPE::DIG);
    const Reg EnableClockOutFrontPanel ("EnClockOutFP", RW::ReadWrite, TYPE::DIG);
    const Reg TrgOutMode               ("TrgOutMode", RW::ReadWrite, TYPE::DIG);
    const Reg GPIOMode                 ("GPIOMode", RW::ReadWrite, TYPE::DIG);
    const Reg SyncOutMode              ("SyncOutMode", RW::ReadWrite, TYPE::DIG);

    const Reg BoardVetoSource          ("BoardVetoSource", RW::ReadWrite, TYPE::DIG);
    const Reg BoardVetoWidth           ("BoardVetoWidth", RW::ReadWrite, TYPE::DIG);
    const Reg BoardVetoPolarity        ("BoardVetoPolarity", RW::ReadWrite, TYPE::DIG);
    const Reg RunDelay                 ("RunDelay", RW::ReadWrite, TYPE::DIG);
    const Reg EnableAutoDisarmACQ      ("EnAutoDisarmAcq", RW::ReadWrite, TYPE::DIG);
    const Reg EnableDataReduction      ("EnDataReduction", RW::ReadWrite, TYPE::DIG);
    const Reg EnableStatisticEvents    ("EnStatEvents", RW::ReadWrite, TYPE::DIG);
    const Reg VolatileClockOutDelay    ("VolatileClockOutDelay", RW::ReadWrite, TYPE::DIG);
    const Reg PermanentClockOutDelay   ("PermanentClockOutDelay", RW::ReadWrite, TYPE::DIG);
    const Reg TestPulsePeriod          ("TestPulsePeriod", RW::ReadWrite, TYPE::DIG);
    const Reg TestPulseWidth           ("TestPulseWidth", RW::ReadWrite, TYPE::DIG);
    const Reg TestPulseLowLevel        ("TestPulseLowLevel", RW::ReadWrite, TYPE::DIG);
    const Reg TestPulseHighLevel       ("TestPulseHighLevel", RW::ReadWrite, TYPE::DIG);
    const Reg ErrorFlagMask            ("ErrorFlagMask", RW::ReadWrite, TYPE::DIG);
    const Reg ErrorFlagDataMask        ("ErrorFlagDataMask", RW::ReadWrite, TYPE::DIG);
    const Reg DACoutMode               ("DACoutMode", RW::ReadWrite, TYPE::DIG);
    const Reg DACoutStaticLevel        ("DACoutStaticLevel", RW::ReadWrite, TYPE::DIG);
    const Reg DACoutChSelect           ("DACoutChSelect", RW::ReadWrite, TYPE::DIG);
    const Reg EnableOffsetCalibration  ("EnOffsetCalibration", RW::ReadWrite, TYPE::DIG);

    /// ========== command
    const Reg Reset               ("Reset", RW::WriteOnly, TYPE::DIG, true);
    const Reg ClearData           ("ClearData", RW::WriteOnly, TYPE::DIG, true); // clear memory, setting not affected
    const Reg ArmACQ              ("ArmAcquisition", RW::WriteOnly, TYPE::DIG, true);
    const Reg DisarmACQ           ("DisarmAcquisition", RW::WriteOnly, TYPE::DIG, true);
    const Reg SoftwareStartACQ    ("SwStartAcquisition", RW::WriteOnly, TYPE::DIG, true); // only when SwStart in StartSource
    const Reg SoftwareStopACQ     ("SwStopAcquisition", RW::WriteOnly, TYPE::DIG, true); // stop ACQ, whatever start source
    const Reg SendSoftwareTrigger ("SendSWTrigger", RW::WriteOnly, TYPE::DIG, true); // only work when Swtrg in the GlobalTriggerSource
    const Reg ReloadCalibration   ("ReloadCalibration", RW::WriteOnly, TYPE::DIG, true); 


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
      //EnableClockOutBackplane  ,
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
    const Reg VGAGain ("VGAGain", RW::ReadWrite, TYPE::VGA); // VX2745 only
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
    const Reg Polarity         ("PulsePolarity", RW::ReadWrite);

    const Reg WaveDataSource              ("WaveDataSource", RW::ReadWrite);
    const Reg RecordLength                ("ChRecordLengthT", RW::ReadWrite);
    const Reg PreTrigger                  ("ChPreTriggerT", RW::ReadWrite);
    const Reg WaveSaving                  ("WaveSaving", RW::ReadWrite);
    const Reg WaveResolution              ("WaveResolution", RW::ReadWrite);
    const Reg TimeFilterRiseTime          ("TimeFilterRiseTimeT", RW::ReadWrite);
    const Reg TimeFilterRetriggerGuard    ("TimeFilterRetriggerGuardT", RW::ReadWrite);
    const Reg EnergyFilterRiseTime        ("EnergyFilterRiseTimeT", RW::ReadWrite);
    const Reg EnergyFilterFlatTop         ("EnergyFilterFlatTopT", RW::ReadWrite);
    const Reg EnergyFilterPoleZero        ("EnergyFilterPoleZeroT", RW::ReadWrite);
    const Reg EnergyFilterPeakingPosition ("EnergyFilterPeakingPosition", RW::ReadWrite);
    const Reg EnergyFilterPeakingAvg      ("EnergyFilterPeakingAvg", RW::ReadWrite);
    const Reg EnergyFilterBaselineAvg     ("EnergyFilterBaselineAvg", RW::ReadWrite);
    const Reg EnergyFilterBaselineGuard   ("EnergyFilterBaselineGuardT", RW::ReadWrite);
    const Reg EnergyFilterFineGain        ("EnergyFilterFineGain", RW::ReadWrite);
    const Reg EnergyFilterPileUpGuard     ("EnergyFilterPileUpGuardT", RW::ReadWrite);
    const Reg EnergyFilterLowFreqFilter   ("EnergyFilterLFLimitation", RW::ReadWrite);
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