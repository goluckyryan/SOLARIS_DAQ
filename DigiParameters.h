#ifndef  DIGITIZER_PARAMETER_H
#define  DIGITIZER_PARAMETER_H

#include <cstdlib>
#include <string>
#include <vector>

enum ANSTYPE {INTEGER, FLOAT, LIST, STR, BYTE, BINARY, NONE};
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
    ANSTYPE ansType;
    std::string answerUnit;
    std::vector<std::pair<std::string, std::string>> answer;
    
  public:
    Reg(){
      name = "";
      readWrite = RW::ReadWrite;
      type = TYPE::CH;
      isCmd = false;
      value = "";
      ansType = ANSTYPE::LIST;
      answerUnit = "";
      answer.clear();
    }
    Reg(std::string para, RW readwrite, 
        TYPE type = TYPE::CH,
        std::vector<std::pair<std::string,std::string>> answer = {},
        ANSTYPE ansType = ANSTYPE::LIST,
        std::string ansUnit = "",
        bool isCmd = false){
      this->name = para;
      this->readWrite = readwrite;
      this->type = type;
      this->isCmd = isCmd;
      this->value = "";
      this->ansType = ansType;
      this->answer = answer;
      this->answerUnit = ansUnit;
    }
    ~Reg(){};

    void        SetValue(std::string sv) { this->value = sv;}
    std::string GetValue() const { return value;}
    RW          ReadWrite() const {return readWrite;}
    TYPE        GetType() const {return type;}
    ANSTYPE     GetAnswerType() const {return ansType;}
    std::string GetUnit() const {return answerUnit;}
    std::vector<std::pair<std::string,std::string>> GetAnswers() const {return answer;}

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


    operator std::string () const {return name;} // this allow Reg kaka("XYZ", true); std::string haha = kaka;

};


//^==================== Some digitizer parameters
namespace PHA{

  const unsigned short TraceStep = 8;
  namespace DIG{
    
    ///============== read only
    const Reg CupVer                   ("CupVer", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::STR);
    const Reg FPGA_firmwareVersion     ("FPGA_FwVer", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::STR);
    const Reg FirmwareType             ("FwType", RW::ReadOnly, TYPE::DIG, {{"DPP_PHA", ""}, {"DPP_ZLE", ""}, {"DPP_PSD", ""}, {"DPP_DAW", ""}, {"DPP_OPEN", ""}, {"Scope", ""}});
    const Reg ModelCode                ("ModelCode", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::STR);
    const Reg PBCode                   ("PBCode", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::STR);
    const Reg ModelName                ("ModelName", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::STR);
    const Reg FromFactor               ("FormFactor", RW::ReadOnly, TYPE::DIG, {{"0", "VME"}, {"1", "VME64X"}, {"2", "DT"}});
    const Reg FamilyCode               ("FamilyCode", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER);
    const Reg SerialNumber             ("SerialNum", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER);
    const Reg PCBrev_MB                ("PCBrev_MB", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER);
    const Reg PCBrev_PB                ("PCBrev_PB", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER);
    const Reg DPP_License              ("License", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::STR);
    const Reg DPP_LicenseStatus        ("LicenseStatus", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::STR);
    const Reg DPP_LicenseRemainingTime ("LicenseRemainingTime", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER);
    const Reg NumberOfChannel          ("NumCh", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER);
    const Reg ADC_bit                  ("ADC_Nbit", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "sec");
    const Reg ADC_SampleRate           ("ADC_SamplRate", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "MS/s");
    const Reg InputDynamicRange        ("InputRange", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "Vpp");
    const Reg InputType                ("InputType", RW::ReadOnly, TYPE::DIG, {{"0","Singled ended"}, {"1", "Differential"}});
    const Reg InputImpedance           ("Zin", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "Ohm");
    const Reg IPAddress                ("IPAddress", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::STR);
    const Reg NetMask                  ("Netmask", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::STR);
    const Reg Gateway                  ("Gateway", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::STR);
    const Reg LED_status               ("LedStatus", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::BINARY, "byte");
    const Reg ACQ_status               ("AcquisitionStatus", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::BINARY, "byte");
    const Reg MaxRawDataSize           ("MaxRawDataSize", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::STR, "byte");
    const Reg TempSensAirIn            ("TempSensAirIn", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg TempSensAirOut           ("TempSensAirOut", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg TempSensCore             ("TempSensCore", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg TempSensFirstADC         ("TempSensFirstADC", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg TempSensLastADC          ("TempSensLastADC", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg TempSensHottestADC       ("TempSensHottestADC", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg TempSensADC0             ("TempSensADC0", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg TempSensADC1             ("TempSensADC1", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg TempSensADC2             ("TempSensADC2", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg TempSensADC3             ("TempSensADC3", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg TempSensADC4             ("TempSensADC4", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg TempSensADC5             ("TempSensADC5", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg TempSensADC6             ("TempSensADC6", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg TempSensADC7             ("TempSensADC7", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");

    const std::vector<Reg> TempSensADC = {TempSensADC0,TempSensADC1,TempSensADC2,TempSensADC3,TempSensADC4,TempSensADC5,TempSensADC6,TempSensADC7};
    const std::vector<Reg> TempSensOthers = {TempSensAirIn,TempSensAirOut,TempSensCore,TempSensFirstADC,TempSensLastADC,TempSensHottestADC};

    const Reg TempSensDCDC             ("TempSensDCDC", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "C");
    const Reg VInSensDCDC              ("VInSensDCDC", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "V");
    const Reg VOutSensDCDC             ("VOutSensDCDC", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "V");
    const Reg IOutSensDCDC             ("IOutSensDCDC", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "Amp");
    const Reg FreqSensCore             ("FreqSensCore", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "Hz");
    const Reg DutyCycleSensDCDC        ("DutyCycleSensDCDC", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "%");
    const Reg SpeedSensFan1            ("SpeedSensFan1", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "rpm");
    const Reg SpeedSensFan2            ("SpeedSensFan2", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::INTEGER, "rpm");
    const Reg ErrorFlags               ("ErrorFlags", RW::ReadOnly, TYPE::DIG, {}, ANSTYPE::BINARY, "byte");
    const Reg BoardReady               ("BoardReady", RW::ReadOnly, TYPE::DIG, {{"True", "No Error"}, {"False", "Error"}});
  
    ///============= read write
    const Reg ClockSource              ("ClockSource", RW::ReadWrite, TYPE::DIG, {{"Internal", "Internal Clock 62.5 MHz"},
                                                                                  {"FPClkIn", "Front Panel Clock Input"}});
    const Reg IO_Level                 ("IOlevel", RW::ReadWrite, TYPE::DIG, {{"NIM", "NIM (0=0V, 1=-0.8V) "}, {"TTL", "TTL (0=0V, 1=3.3V)"}});
    const Reg StartSource              ("StartSource", RW::ReadWrite, TYPE::DIG, {{"EncodedClkIn", "CLK-IN/SYNC"},
                                                                                  {"SINlevel", "S-IN Level"},
                                                                                  {"SINedge", "S-IN Edge"},
                                                                                  {"SWcmd", "Software"},
                                                                                  {"LVDS", "LVDS"}}, ANSTYPE::STR); 
    const Reg GlobalTriggerSource      ("GlobalTriggerSource", RW::ReadWrite, TYPE::DIG,{{"TrgIn",      "TRG-IN" },    
                                                                                         {"SwTrg",      "Software" },
                                                                                         {"GPIO",       "GPIO" },
                                                                                         {"TestPulse",  "Test Pulse" },
                                                                                         {"LVDS",       "LVDS"}}, ANSTYPE::STR);

    const Reg BusyInSource             ("BusyInSource", RW::ReadWrite, TYPE::DIG, {{"Disabled","Disabled"},
                                                                                   {"SIN", "SIN"},
                                                                                   {"GPIO", "GPIO"},
                                                                                   {"LVDS", "LVDS"}});
    //const Reg EnableClockOutBackplane  ("EnClockOutP0", RW::ReadWrite, TYPE::DIG);
    const Reg EnableClockOutFrontPanel ("EnClockOutFP", RW::ReadWrite, TYPE::DIG, {{"True", "Enable"}, {"False", "Disabled"}});
    const Reg TrgOutMode               ("TrgOutMode", RW::ReadWrite, TYPE::DIG, {{"Disabled",  "Disabled"}, 
                                                                                 {"TRGIN",     "TRG-IN"}, 
                                                                                 {"SwTrg",     "Software Trigger"}, 
                                                                                 {"LVDS",      "LVDS"}, 
                                                                                 {"Run",       "Run Signal"}, 
                                                                                 {"RefClk",    "Reference Clock"}, 
                                                                                 {"TestPulse", "Test Pulse"}, 
                                                                                 {"Busy",      "Busy Signal"}, 
                                                                                 {"Fixed0",    "0-level"}, 
                                                                                 {"Fixed1",    "1-level"}, 
                                                                                 {"SyncIn",    "SyncIn Signal"}, 
                                                                                 {"SIN",       "S-IN Signal"}, 
                                                                                 {"GPIO",      "GPIO Signal"}, 
                                                                                 {"AcceptTrg", "Acceped Trigger Signal"}, 
                                                                                 {"TrgClk",    "Trigger Clock"}});
    const Reg GPIOMode                 ("GPIOMode", RW::ReadWrite, TYPE::DIG, {{"Disabled",  "Disabled"}, 
                                                                               {"TRGIN",     "TRG-IN"}, 
                                                                               {"P0",        "Back Plane"}, 
                                                                               {"SIN",       "S-IN Signal"}, 
                                                                               {"LVDS",      "LVDS Trigger"}, 
                                                                               {"SwTrg",     "Software Trigger"}, 
                                                                               {"Run",       "Run Signal"}, 
                                                                               {"RefClk",    "Referece Clock"}, 
                                                                               {"TestPulse", "Test Pulse"}, 
                                                                               {"Busy",      "Busy Signal"}, 
                                                                               {"Fixed0",    "0-Level"}, 
                                                                               {"Fixed1",    "1-Level"}});
    const Reg SyncOutMode              ("SyncOutMode", RW::ReadWrite, TYPE::DIG, {{"Disabled",  "Disabled"}, 
                                                                                  {"SyncIn",    "Sync-In Signal"}, 
                                                                                  {"TestPulse", "Test Pulse"}, 
                                                                                  {"IntClk",    "Internal Clock 62.5MHz"}, 
                                                                                  {"Run",       "Run Signal"} });
    const Reg BoardVetoSource          ("BoardVetoSource", RW::ReadWrite, TYPE::DIG, {{"Disabled", "Disabled"},
                                                                                      {"SIN",      "S-IN"}, 
                                                                                      {"LVDS",     "LVDS"}, 
                                                                                      {"GPIO",     "GPIO"}, 
                                                                                      {"P0",       "Back Plane"}});
    const Reg BoardVetoWidth           ("BoardVetoWidth", RW::ReadWrite, TYPE::DIG, {{"0", ""}, {"34359738360", ""}, {"1", ""}}, ANSTYPE::INTEGER, "ns");
    const Reg BoardVetoPolarity        ("BoardVetoPolarity", RW::ReadWrite, TYPE::DIG, {{"ActiveHigh", "High"}, {"ActiveLow", "Low"}});
    const Reg RunDelay                 ("RunDelay", RW::ReadWrite, TYPE::DIG, {{"0", ""}, {"524280", ""}, {"1", ""}},  ANSTYPE::INTEGER, "ns");
    const Reg EnableAutoDisarmACQ      ("EnAutoDisarmAcq", RW::ReadWrite, TYPE::DIG, {{"True", "Enabled"}, {"False", "Disabled"}});
    const Reg EnableDataReduction      ("EnDataReduction", RW::ReadWrite, TYPE::DIG, {{"False", "Disabled"}, {"True", "Enabled"}});
    const Reg EnableStatisticEvents    ("EnStatEvents", RW::ReadWrite, TYPE::DIG, {{"False", "Disabled"}, {"True", "Enabled"}});
    const Reg VolatileClockOutDelay    ("VolatileClockOutDelay", RW::ReadWrite, TYPE::DIG, {{"-18888.888", ""}, {"18888.888", ""}, {"74.074", ""}}, ANSTYPE::FLOAT, "ps");
    const Reg PermanentClockOutDelay   ("PermanentClockOutDelay", RW::ReadWrite, TYPE::DIG, {{"-18888.888", ""}, {"18888.888", ""}, {"74.074", ""}},  ANSTYPE::FLOAT, "ps");
    const Reg TestPulsePeriod          ("TestPulsePeriod", RW::ReadWrite, TYPE::DIG, {{"0", ""},{"34359738360", ""}, {"8", ""}}, ANSTYPE::INTEGER, "ns");
    const Reg TestPulseWidth           ("TestPulseWidth", RW::ReadWrite, TYPE::DIG,  {{"0", ""},{"34359738360", ""}, {"8", ""}}, ANSTYPE::INTEGER, "ns");
    const Reg TestPulseLowLevel        ("TestPulseLowLevel", RW::ReadWrite, TYPE::DIG, {{"0", ""},{"65535", ""}, {"1", ""}}, ANSTYPE::INTEGER, "ns");
    const Reg TestPulseHighLevel       ("TestPulseHighLevel", RW::ReadWrite, TYPE::DIG, {{"0", ""},{"65535", ""}, {"1", ""}}, ANSTYPE::INTEGER, "ns");
    const Reg ErrorFlagMask            ("ErrorFlagMask", RW::ReadWrite, TYPE::DIG, {}, ANSTYPE::BINARY);
    const Reg ErrorFlagDataMask        ("ErrorFlagDataMask", RW::ReadWrite, TYPE::DIG, {}, ANSTYPE::BINARY);
    const Reg DACoutMode               ("DACoutMode", RW::ReadWrite, TYPE::DIG, {{"Static", "DAC fixed level"},
                                                                                 {"ChInput", "From Channel"},
                                                                                 {"ChSum", "Sum of all Channels"},
                                                                                 {"OverThrSum", "Number of Channels triggered"},
                                                                                 {"Ramp", "14-bit counter"},
                                                                                 {"Sin5MHz", "5 MHz Sin wave"},
                                                                                 {"Square", "Test Pulse"}});
    const Reg DACoutStaticLevel        ("DACoutStaticLevel", RW::ReadWrite, TYPE::DIG, {{"0", ""}, {"16383", ""}, {"1",""}}, ANSTYPE::INTEGER, "units");
    const Reg DACoutChSelect           ("DACoutChSelect", RW::ReadWrite, TYPE::DIG, {{"0", ""}, {"64", ""}, {"1",""}}, ANSTYPE::INTEGER);
    const Reg EnableOffsetCalibration  ("EnOffsetCalibration", RW::ReadWrite, TYPE::DIG, {{"True", "Applied Cali."}, {"False", "No Cali."}});

    const Reg ITLAMainLogic          ("ITLAMainLogic",   RW::ReadWrite, TYPE::DIG, {{"OR", "OR"},{"AND", "AND"}, {"Majority", "Majority"}});
    const Reg ITLAMajorityLev        ("ITLAMajorityLev", RW::ReadWrite, TYPE::DIG, {{"1", ""},{"63", ""}, {"1", ""}}, ANSTYPE::INTEGER);
    const Reg ITLAPairLogic          ("ITLAPairLogic",   RW::ReadWrite, TYPE::DIG, {{"OR", "OR"},{"AND", "AND"}, {"NONE", "NONE"}});
    const Reg ITLAPolarity           ("ITLAPolarity",    RW::ReadWrite, TYPE::DIG, {{"Direct", "Direct"},{"Inverted", "Inverted"}});
    const Reg ITLAMask               ("ITLAMask",        RW::ReadWrite, TYPE::DIG, {}, ANSTYPE::BYTE, "64-bit");
    const Reg ITLAGateWidth          ("ITLAGateWidth",   RW::ReadWrite, TYPE::DIG, {{"0", ""}, {"524280", ""}, {"8", ""}}, ANSTYPE::INTEGER, "ns");

    const Reg ITLBMainLogic          ("ITLBMainLogic",   RW::ReadWrite, TYPE::DIG, {{"OR", "OR"},{"AND", "AND"}, {"Majority", "Majority"}});
    const Reg ITLBMajorityLev        ("ITLBMajorityLev", RW::ReadWrite, TYPE::DIG, {{"1", ""},{"63", ""}, {"1", ""}}, ANSTYPE::INTEGER);
    const Reg ITLBPairLogic          ("ITLBPairLogic",   RW::ReadWrite, TYPE::DIG, {{"OR", "OR"},{"AND", "AND"}, {"NONE", "NONE"}});
    const Reg ITLBPolarity           ("ITLBPolarity",    RW::ReadWrite, TYPE::DIG, {{"Direct", "Direct"},{"Inverted", "Inverted"}});
    const Reg ITLBMask               ("ITLBMask",        RW::ReadWrite, TYPE::DIG, {}, ANSTYPE::BYTE, "64-bit");
    const Reg ITLBGateWidth          ("ITLBGateWidth",   RW::ReadWrite, TYPE::DIG, {{"0", ""}, {"524280", ""}, {"8", ""}}, ANSTYPE::INTEGER, "ns");


    const Reg LVDSIOReg   ("LVDSIOReg",   RW::ReadWrite, TYPE::DIG, {}, ANSTYPE::STR);
    //const Reg LVDSTrgMask ("lvdstrgmask", RW::ReadWrite, TYPE::DIG, {}, ANSTYPE::BYTE, "64-bit");

    /// ========== command
    const Reg Reset               ("Reset", RW::WriteOnly, TYPE::DIG, {}, ANSTYPE::NONE, "", true);
    const Reg ClearData           ("ClearData", RW::WriteOnly, TYPE::DIG, {},  ANSTYPE::NONE, "", true); // clear memory, setting not affected
    const Reg ArmACQ              ("ArmAcquisition", RW::WriteOnly, TYPE::DIG, {},  ANSTYPE::NONE, "", true);
    const Reg DisarmACQ           ("DisarmAcquisition", RW::WriteOnly, TYPE::DIG, {}, ANSTYPE::NONE, "", true);
    const Reg SoftwareStartACQ    ("SwStartAcquisition", RW::WriteOnly, TYPE::DIG, {}, ANSTYPE::NONE, "", true); // only when SwStart in StartSource
    const Reg SoftwareStopACQ     ("SwStopAcquisition", RW::WriteOnly, TYPE::DIG, {}, ANSTYPE::NONE, "", true); // stop ACQ, whatever start source
    const Reg SendSoftwareTrigger ("SendSWTrigger", RW::WriteOnly, TYPE::DIG, {}, ANSTYPE::NONE, "", true); // only work when Swtrg in the GlobalTriggerSource
    const Reg ReloadCalibration   ("ReloadCalibration", RW::WriteOnly, TYPE::DIG, {}, ANSTYPE::NONE, "", true); 


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
      ADC_bit                  ,
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
      EnableOffsetCalibration  ,
      ITLAMainLogic            ,
      ITLAMajorityLev          ,
      ITLAPairLogic            ,
      ITLAPolarity             ,
      ITLAMask                 ,
      ITLAGateWidth            ,
      ITLBMainLogic            ,
      ITLBMajorityLev          ,
      ITLBPairLogic            ,
      ITLBPolarity             ,
      ITLBMask                 ,
      ITLBGateWidth            ,
      LVDSIOReg                
      //LVDSTrgMask              
    };


  }

  namespace VGA{
    const Reg VGAGain ("VGAGain", RW::ReadWrite, TYPE::VGA, {{"0", ""},{"40", ""}, {"0.5",""}}, ANSTYPE::INTEGER, "dB"); // VX2745 only
  }

  namespace LVDS{

    const Reg LVDSMode ("LVDSMode", RW::ReadWrite, TYPE::LVDS, {{"SelfTriggers", "Self-Trigger"},
                                                                {"Sync", "Sync"},
                                                                {"IORegister", "IORegister"}});

    const Reg LVDSDirection ("LVDSDirection", RW::ReadWrite, TYPE::LVDS, {{"Input", "Input"},
                                                                          {"Output", "Output"}});

    const std::vector<Reg> AllSettings = {
      LVDSMode      ,
      LVDSDirection 
    };

  }

  namespace CH{

    /// ========= red only
    const Reg SelfTrgRate          ("SelfTrgRate", RW::ReadOnly, TYPE::CH, {}, ANSTYPE::INTEGER, "Hz");
    const Reg ChannelStatus        ("ChStatus", RW::ReadOnly, TYPE::CH, {}, ANSTYPE::STR);
    const Reg GainFactor           ("GainFactor", RW::ReadOnly, TYPE::CH, {}, ANSTYPE::FLOAT);
    const Reg ADCToVolts           ("ADCToVolts", RW::ReadOnly, TYPE::CH, {}, ANSTYPE::FLOAT);
    const Reg Energy_Nbit          ("Energy_Nbit", RW::ReadOnly, TYPE::CH, {}, ANSTYPE::STR);
    const Reg ChannelRealtime      ("ChRealtimeMonitor", RW::ReadOnly, TYPE::CH, {}, ANSTYPE::STR); // when called, update DeadTime, TriggerCount, SaveCount, and WaveCount
    const Reg ChannelDeadtime      ("ChDeadtimeMonitor", RW::ReadOnly, TYPE::CH, {}, ANSTYPE::STR);
    const Reg ChannelTriggerCount  ("ChTriggerCnt", RW::ReadOnly, TYPE::CH, {}, ANSTYPE::STR);
    const Reg ChannelSavedCount    ("ChSavedEventCnt", RW::ReadOnly, TYPE::CH, {}, ANSTYPE::STR);
    const Reg ChannelWaveCount     ("ChWaveCnt", RW::ReadOnly, TYPE::CH, {}, ANSTYPE::STR);

    /// ======= read write
    const Reg ChannelEnable    ("ChEnable", RW::ReadWrite, TYPE::CH, {{"True", "Enabled"}, {"False", "Disabled"}});
    const Reg DC_Offset        ("DCOffset", RW::ReadWrite, TYPE::CH, {{"0", ""}, {"100", ""}, {"1",""}}, ANSTYPE::INTEGER, "%"); 
    const Reg TriggerThreshold ("TriggerThr", RW::ReadWrite, TYPE::CH, {{"0", ""},{"8191", ""}, {"1",""}}, ANSTYPE::INTEGER);
    const Reg Polarity         ("PulsePolarity", RW::ReadWrite, TYPE::CH, {{"Positive", "Pos. +"},{"Negative", "Neg. -"}});

    const Reg WaveDataSource              ("WaveDataSource", RW::ReadWrite, TYPE::CH, {{"ADC_DATA",         "Input ADC"}, 
                                                                                       {"ADC_TEST_TOGGLE",  "ADC produces TOGGLE signal"}, 
                                                                                       {"ADC_TEST_RAMP",    "ADC produces RAMP signal"}, 
                                                                                       {"ADC_TEST_SIN",     "ADC produce SIN signal"}, 
                                                                                       {"Ramp",             "Ramp generator"}, 
                                                                                       {"SquareWave",       "Test Pusle (Square Wave)"}  });
    const Reg RecordLength                ("ChRecordLengthT", RW::ReadWrite, TYPE::CH, {{"32", ""}, {"64800", ""}, {"8",""}}, ANSTYPE::INTEGER, "ns");
    const Reg PreTrigger                  ("ChPreTriggerT", RW::ReadWrite, TYPE::CH, {{"32", ""}, {"32000", ""}, {"8",""}}, ANSTYPE::INTEGER, "ns");
    const Reg WaveSaving                  ("WaveSaving", RW::ReadWrite, TYPE::CH, {{"Always", "Always"}, {"OnRequest", "On Request"}});
    const Reg WaveResolution              ("WaveResolution", RW::ReadWrite, TYPE::CH, {{"RES8", " 8 ns"}, 
                                                                                       {"RES16","16 ns"},
                                                                                       {"RES32","32 ns"},
                                                                                       {"RES64","64 ns"}});
    const Reg TimeFilterRiseTime          ("TimeFilterRiseTimeT", RW::ReadWrite, TYPE::CH, {{"32", ""},{"2000", ""}, {"8",""}}, ANSTYPE::INTEGER, "ns");
    const Reg TimeFilterRetriggerGuard    ("TimeFilterRetriggerGuardT", RW::ReadWrite, TYPE::CH, {{"0", ""},{"8000", ""}, {"8",""}}, ANSTYPE::INTEGER, "ns");
    const Reg EnergyFilterRiseTime        ("EnergyFilterRiseTimeT", RW::ReadWrite, TYPE::CH, {{"32", ""},{"13000", ""}, {"8",""}}, ANSTYPE::INTEGER, "ns");
    const Reg EnergyFilterFlatTop         ("EnergyFilterFlatTopT", RW::ReadWrite, TYPE::CH, {{"32", ""},{"3000", ""}, {"8",""}}, ANSTYPE::INTEGER, "ns");
    const Reg EnergyFilterPoleZero        ("EnergyFilterPoleZeroT", RW::ReadWrite, TYPE::CH, {{"32", ""},{"524000", ""}, {"8",""}}, ANSTYPE::INTEGER, "ns");
    const Reg EnergyFilterPeakingPosition ("EnergyFilterPeakingPosition", RW::ReadWrite, TYPE::CH, {{"10", ""},{"90", ""}, {"1",""}}, ANSTYPE::INTEGER, "%");
    const Reg EnergyFilterPeakingAvg      ("EnergyFilterPeakingAvg", RW::ReadWrite, TYPE::CH, {{"OneShot",   "1 sample"},
                                                                                               {"LowAVG",    "4 samples"},
                                                                                               {"MediumAVG", "16 samples"},
                                                                                               {"HighAVG",   "64 samples"}});
    const Reg EnergyFilterBaselineAvg     ("EnergyFilterBaselineAvg", RW::ReadWrite, TYPE::CH, {{"Fixed",    "0 sample"},
                                                                                               {"VeryLow",   "16 samples"},
                                                                                               {"Low",       "64 samples"},
                                                                                               {"MediumLow", "256 samples"},
                                                                                               {"Medium",    "1024 samples"},
                                                                                               {"MediumHigh","4096 samples"},
                                                                                               {"High",      "16384 samples"}});
    const Reg EnergyFilterBaselineGuard   ("EnergyFilterBaselineGuardT", RW::ReadWrite, TYPE::CH, {{"0", ""},{"8000", ""}, {"8",""}}, ANSTYPE::INTEGER, "ns");
    const Reg EnergyFilterFineGain        ("EnergyFilterFineGain", RW::ReadWrite, TYPE::CH, {{"0", ""},{"10", ""}, {"0.001",""}}, ANSTYPE::FLOAT);
    const Reg EnergyFilterPileUpGuard     ("EnergyFilterPileUpGuardT", RW::ReadWrite, TYPE::CH, {{"0", ""},{"64000", ""}, {"8",""}}, ANSTYPE::INTEGER, "ns");
    const Reg EnergyFilterLowFreqFilter   ("EnergyFilterLFLimitation", RW::ReadWrite, TYPE::CH, {{"Off", "Disabled"}, {"On", "Enabled"}});
    const Reg WaveAnalogProbe0            ("WaveAnalogProbe0", RW::ReadWrite, TYPE::CH, {{"ADCInput",                   "ADC Input"}, 
                                                                                         {"TimeFilter",                 "Time Filter"}, 
                                                                                         {"EnergyFilter",               "Trapazoid"}, 
                                                                                         {"EnergyFilterBase",           "Trap. Baseline"}, 
                                                                                         {"EnergyFilterMinusBaseline",  "Trap. - Baseline"}});
    const Reg WaveAnalogProbe1            ("WaveAnalogProbe1", RW::ReadWrite, TYPE::CH, {{"ADCInput",                   "ADC Input"}, 
                                                                                         {"TimeFilter",                 "Time Filter"}, 
                                                                                         {"EnergyFilter",               "Trapazoid"}, 
                                                                                         {"EnergyFilterBase",           "Trap. Baseline"}, 
                                                                                         {"EnergyFilterMinusBaseline",  "Trap. - Baseline"}});
    const Reg WaveDigitalProbe0           ("WaveDigitalProbe0", RW::ReadWrite, TYPE::CH, {{"Trigger",                    "Trigger"},              
                                                                                          {"TimeFilterArmed",            "Time Filter Armed"},    
                                                                                          {"ReTriggerGuard",             "ReTrigger Guard"},      
                                                                                          {"EnergyFilterBaselineFreeze", "Trap. basline Freeze"}, 
                                                                                          {"EnergyFilterPeaking",        "Peaking"},  
                                                                                          {"EnergyFilterPeakReady",      "Peak Ready"},           
                                                                                          {"EnergyFilterPileUpGuard",    "Pile-up Guard"},        
                                                                                          {"EventPileUp",                "Event Pile Up"},        
                                                                                          {"ADCSaturation",              "ADC Saturate"},         
                                                                                          {"ADCSaturationProtection",    "ADC Sat. Protection"},  
                                                                                          {"PostSaturationEvent",        "Post Sat. Event"},      
                                                                                          {"EnergylterSaturation",       "Trap. Saturate"},       
                                                                                          {"AcquisitionInhibit",         "ACQ Inhibit"}    });
    const Reg WaveDigitalProbe1           ("WaveDigitalProbe1", RW::ReadWrite, TYPE::CH, {{"Trigger",                    "Trigger"},              
                                                                                          {"TimeFilterArmed",            "Time Filter Armed"},    
                                                                                          {"ReTriggerGuard",             "ReTrigger Guard"},      
                                                                                          {"EnergyFilterBaselineFreeze", "Trap. basline Freeze"}, 
                                                                                          {"EnergyFilterPeaking",        "Peaking"},  
                                                                                          {"EnergyFilterPeakReady",      "Peak Ready"},           
                                                                                          {"EnergyFilterPileUpGuard",    "Pile-up Guard"},        
                                                                                          {"EventPileUp",                "Event Pile Up"},        
                                                                                          {"ADCSaturation",              "ADC Saturate"},         
                                                                                          {"ADCSaturationProtection",    "ADC Sat. Protection"},  
                                                                                          {"PostSaturationEvent",        "Post Sat. Event"},      
                                                                                          {"EnergylterSaturation",       "Trap. Saturate"},       
                                                                                          {"AcquisitionInhibit",         "ACQ Inhibit"}    });
    const Reg WaveDigitalProbe2           ("WaveDigitalProbe2", RW::ReadWrite, TYPE::CH, {{"Trigger",                    "Trigger"},              
                                                                                          {"TimeFilterArmed",            "Time Filter Armed"},    
                                                                                          {"ReTriggerGuard",             "ReTrigger Guard"},      
                                                                                          {"EnergyFilterBaselineFreeze", "Trap. basline Freeze"}, 
                                                                                          {"EnergyFilterPeaking",        "Peaking"},  
                                                                                          {"EnergyFilterPeakReady",      "Peak Ready"},           
                                                                                          {"EnergyFilterPileUpGuard",    "Pile-up Guard"},        
                                                                                          {"EventPileUp",                "Event Pile Up"},        
                                                                                          {"ADCSaturation",              "ADC Saturate"},         
                                                                                          {"ADCSaturationProtection",    "ADC Sat. Protection"},  
                                                                                          {"PostSaturationEvent",        "Post Sat. Event"},      
                                                                                          {"EnergylterSaturation",       "Trap. Saturate"},       
                                                                                          {"AcquisitionInhibit",         "ACQ Inhibit"}    });
    const Reg WaveDigitalProbe3           ("WaveDigitalProbe3", RW::ReadWrite, TYPE::CH, {{"Trigger",                    "Trigger"},              
                                                                                          {"TimeFilterArmed",            "Time Filter Armed"},    
                                                                                          {"ReTriggerGuard",             "ReTrigger Guard"},      
                                                                                          {"EnergyFilterBaselineFreeze", "Trap. basline Freeze"}, 
                                                                                          {"EnergyFilterPeaking",        "Peaking"},  
                                                                                          {"EnergyFilterPeakReady",      "Peak Ready"},           
                                                                                          {"EnergyFilterPileUpGuard",    "Pile-up Guard"},        
                                                                                          {"EventPileUp",                "Event Pile Up"},        
                                                                                          {"ADCSaturation",              "ADC Saturate"},         
                                                                                          {"ADCSaturationProtection",    "ADC Sat. Protection"},  
                                                                                          {"PostSaturationEvent",        "Post Sat. Event"},      
                                                                                          {"EnergylterSaturation",       "Trap. Saturate"},       
                                                                                          {"AcquisitionInhibit",         "ACQ Inhibit"}    });

    const std::vector<Reg> AnalogProbe  = {WaveAnalogProbe0, WaveAnalogProbe1};
    const std::vector<Reg> DigitalProbe = {WaveDigitalProbe0, WaveDigitalProbe1, WaveDigitalProbe2, WaveDigitalProbe3};


    const Reg EventTriggerSource      ("EventTriggerSource", RW::ReadWrite, TYPE::CH, {{"GlobalTriggerSource",   "Global Trigger Source"}, 
                                                                                       {"TRGIN",                 "TRG-IN"}, 
                                                                                       {"SWTrigger",             "Software Trigger"}, 
                                                                                       {"ChSelfTrigger",         "Channel Self-Trigger"}, 
                                                                                       {"Ch64Trigger",           "Channel 64-Trigger"}, 
                                                                                       {"Disabled",              "Disabled"}});
    const Reg ChannelsTriggerMask     ("ChannelsTriggerMask", RW::ReadWrite, TYPE::CH, {},  ANSTYPE::BYTE, "64-bit" );
    const Reg ChannelVetoSource       ("ChannelVetoSource", RW::ReadWrite, TYPE::CH, {{"BoardVeto", "Board Veto"},
                                                                                      {"ADCOverSaturation", "ADC Over Saturation"},
                                                                                      {"ADCUnderSaturation", "ADC Under Saturation"},
                                                                                      {"Disabled", "Disabled"}});
    const Reg WaveTriggerSource       ("WaveTriggerSource", RW::ReadWrite, TYPE::CH, {{"GlobalTriggerSource",  "Global Trigger Source"}, 
                                                                                      {"TRGIN",                "TRG-IN"}, 
                                                                                      {"ExternalInhibit",      "External Inhibit"}, 
                                                                                      {"ADCUnderSaturation",   "ADC Under Saturation"}, 
                                                                                      {"ADCOverSaturation",    "ADC Over Saturation"}, 
                                                                                      {"SWTrigger",            "Software Trigger"}, 
                                                                                      {"ChSelfTrigger",        "Channel Self-Trigger"}, 
                                                                                      {"Ch64Trigger",          "Channel 64-Trigger"}, 
                                                                                      {"Disabled",             "Disabled"}});

    const Reg EventSelector           ("EventSelector", RW::ReadWrite, TYPE::CH, {{"All", "All"},
                                                                                  {"Pileup", "Pile up"},
                                                                                  {"EnergySkim", "Energy Skim"}});
    const Reg WaveSelector            ("WaveSelector", RW::ReadWrite, TYPE::CH, {{"All", "All wave"},
                                                                                  {"Pileup", "Only Pile up"},
                                                                                  {"EnergySkim", "Only in Energy Skim Range"}});
    const Reg CoincidenceMask         ("CoincidenceMask", RW::ReadWrite, TYPE::CH, {{"Disabled", "Disabled"},
                                                                                    {"Ch64Trigger", "Channel 64-Trigger"},
                                                                                    {"TRGIN", "TRG-IN"},
                                                                                    {"GlobalTriggerSource", "Global Trigger"},
                                                                                    {"ITLA", "ITLA"},
                                                                                    {"ITLB", "ITLB"}});
    const Reg AntiCoincidenceMask     ("AntiCoincidenceMask", RW::ReadWrite, TYPE::CH,{{"Disabled", "Disabled"},
                                                                                       {"Ch64Trigger", "Channel 64-Trigger"},
                                                                                       {"TRGIN", "TRG-IN"},
                                                                                       {"GlobalTriggerSource", "Global Trigger"},
                                                                                       {"ITLA", "ITLA"},
                                                                                       {"ITLB", "ITLB"}});
    const Reg CoincidenceLength       ("CoincidenceLengthT", RW::ReadWrite, TYPE::CH, {{"0", ""},{"524280", ""}, {"8", ""}}, ANSTYPE::INTEGER, "ns");
    const Reg CoincidenceLengthSample ("CoincidenceLengthS", RW::ReadWrite, TYPE::CH, {{"0", ""},{"65535", ""}, {"1", ""}}, ANSTYPE::INTEGER, "sample");

    const Reg ADCVetoWidth       ("ADCVetoWidth", RW::ReadWrite, TYPE::CH, {{"0", ""}, {"524280", ""}, {"1", ""}},  ANSTYPE::INTEGER, "ns");

    const Reg EnergySkimLowDiscriminator  ("EnergySkimLowDiscriminator", RW::ReadWrite, TYPE::CH,  {{"0", ""}, {"65534", ""}, {"1", ""}}, ANSTYPE::INTEGER);
    const Reg EnergySkimHighDiscriminator ("EnergySkimHighDiscriminator", RW::ReadWrite, TYPE::CH,  {{"0", ""}, {"65534", ""}, {"1", ""}}, ANSTYPE::INTEGER);

    const Reg RecordLengthSample              ("ChRecordLengthS", RW::ReadWrite, TYPE::CH, {{"4", ""},{"8100", ""}, {"1", ""}}, ANSTYPE::INTEGER, "sample");
    const Reg PreTriggerSample                ("ChPreTriggerS", RW::ReadWrite, TYPE::CH, {{"4", ""},{"4000", ""}, {"1", ""}}, ANSTYPE::INTEGER, "sample");
    const Reg TimeFilterRiseTimeSample        ("TimeFilterRiseTimeS", RW::ReadWrite, TYPE::CH, {{"4", ""},{"250", ""}, {"1", ""}}, ANSTYPE::INTEGER, "sample");
    const Reg TimeFilterRetriggerGuardSample  ("TimeFilterRetriggerGuardS", RW::ReadWrite, TYPE::CH, {{"0", ""},{"1000", ""}, {"1", ""}}, ANSTYPE::INTEGER, "sample");
    const Reg EnergyFilterRiseTimeSample      ("EnergyFilterRiseTimeS", RW::ReadWrite, TYPE::CH, {{"4", ""},{"1625", ""}, {"1", ""}}, ANSTYPE::INTEGER, "sample");
    const Reg EnergyFilterFlatTopSample       ("EnergyFilterFlatTopS", RW::ReadWrite, TYPE::CH, {{"4", ""},{"375", ""}, {"1", ""}}, ANSTYPE::INTEGER, "sample");
    const Reg EnergyFilterPoleZeroSample      ("EnergyFilterPoleZeroS", RW::ReadWrite, TYPE::CH, {{"4", ""},{"65500", ""}, {"1", ""}}, ANSTYPE::INTEGER, "sample");
    const Reg EnergyFilterBaselineGuardSample ("EnergyFilterBaselineGuardS", RW::ReadWrite, TYPE::CH, {{"0", ""},{"1000", ""}, {"1", ""}}, ANSTYPE::INTEGER, "sample");
    const Reg EnergyFilterPileUpGuardSample   ("EnergyFilterPileUpGuardS", RW::ReadWrite, TYPE::CH, {{"0", ""},{"8000", ""}, {"1", ""}}, ANSTYPE::INTEGER, "sample");

    const Reg ITLConnect             ("ITLConnect",    RW::ReadWrite, TYPE::CH, {{"Disabled", "Disabled"},{"ITLA", "ITLA"}, {"ITLB", "ITLB"}});

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
      EnergyFilterPileUpGuardSample    ,
      ITLConnect
    };

  }

};


#endif