#include "digiSettings.h"

#include <QLabel>

DigiSettings::DigiSettings(Digitizer2Gen * digi, unsigned short nDigi, QWidget * parent) : QWidget(parent){

  qDebug() << "DigiSettings constructor";

  setWindowTitle("Digitizers Settings");
  setGeometry(0, 0, 1900, 1000);
  //setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);


  this->digi = digi;
  this->nDigi = nDigi;

  std::vector<std::vector<std::string>> info = {{"Serial Num : ",            "/par/SerialNum"},
                                                {"IP : ",                    "/par/IPAddress"},
                                                {"Model Name : ",            "/par/ModelName"},
                                                {"FPGA version : ",          "/par/FPGA_FwVer"},
                                                {"DPP Type : ",              "/par/FwType"},
                                                {"CUP version : ",           "/par/cupver"},
                                                {"ADC bits : ",              "/par/ADC_Nbit"},
                                                {"ADC rate [Msps] : ",       "/par/ADC_SamplRate"},
                                                {"Num. of Channel : ",       "/par/NumCh"},
                                                {"Input range [Vpp] : ",     "/par/InputRange"},
                                                {"Input Type : ",            "/par/InputType"},
                                                {"Input Impedance [Ohm] : ", "/par/Zin"}
                                               };

  QVBoxLayout * mainLayout = new QVBoxLayout(this); this->setLayout(mainLayout);
  QTabWidget * tabWidget = new QTabWidget(this); mainLayout->addWidget(tabWidget);

  //============ Tab for each digitizer
  for(unsigned short iDigi = 0; iDigi < this->nDigi; iDigi++){

    QWidget * tab = new QWidget(tabWidget);
    QScrollArea * scrollArea = new QScrollArea(this); scrollArea->setWidget(tab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    tabWidget->addTab(scrollArea, "Digi-" + QString::number(digi->GetSerialNumber()));
    
    QGridLayout *tabLayout = new QGridLayout(tab); tab->setLayout(tabLayout);

    {//-------- Group of Digitizer Info
      QGroupBox * infoBox = new QGroupBox("Board Info", tab);
      QGridLayout * infoLayout = new QGridLayout(infoBox);
      tabLayout->addWidget(infoBox, 0, 0);
      
      const unsigned short nRow = 4;
      for( unsigned short j = 0; j < (unsigned short) info.size(); j++){
        QLabel * lab = new QLabel(QString::fromStdString(info[j][0]), tab);
        lab->setAlignment(Qt::AlignRight);
        QLineEdit * txt = new QLineEdit(tab);
        txt->setReadOnly(true);
        txt->setText(QString::fromStdString(digi->ReadValue(info[j][1].c_str())));
        infoLayout->addWidget(lab, j%nRow, 2*(j/nRow));
        infoLayout->addWidget(txt, j%nRow, 2*(j/nRow) +1);
      }
    }

    {//------- Group Board status
      QGroupBox * statusBox = new QGroupBox("Board Status", tab);
      QGridLayout * statusLayout = new QGridLayout(statusBox);
      tabLayout->addWidget(statusBox, 0, 1);
    }

    {//------- Group digitizer settings
      QGroupBox * digiBox = new QGroupBox("Board Settings", tab);
      QGridLayout * boardLayout = new QGridLayout(digiBox);
      tabLayout->addWidget(digiBox, 1, 0);
        
      int rowId = 0;
      //-------------------------------------
      QPushButton * bnResetBd = new QPushButton("Reset Board", tab);
      boardLayout->addWidget(bnResetBd, rowId, 0, 1, 2);
      connect(bnResetBd, &QPushButton::clicked, this, &DigiSettings::onReset);
      
      QPushButton * bnDefaultSetting = new QPushButton("Set Default Settings", tab);
      boardLayout->addWidget(bnDefaultSetting, rowId, 2, 1, 2);
      connect(bnDefaultSetting, &QPushButton::clicked, this, &DigiSettings::onDefault);

      //-------------------------------------
      rowId ++;
      QLabel * lbClockSource = new QLabel("Clock Source :", tab);
      lbClockSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbClockSource, rowId, 0);

      QComboBox * comClockSource = new QComboBox(tab);
      boardLayout->addWidget(comClockSource, rowId, 1, 1, 2);
      comClockSource->addItem("Internal 62.5 MHz");
      comClockSource->addItem("Front Panel Clock input");

      //-------------------------------------
      rowId ++;
      QLabel * lbStartSource = new QLabel("Start Source :", tab);
      lbStartSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbStartSource, rowId, 0);

      QCheckBox * cbStartSource1 = new QCheckBox("EncodedClkIn", tab);
      boardLayout->addWidget(cbStartSource1, rowId, 1);
      QCheckBox * cbStartSource2 = new QCheckBox("SIN Level", tab);
      boardLayout->addWidget(cbStartSource2, rowId, 2);
      QCheckBox * cbStartSource3 = new QCheckBox("SIN Edge", tab);
      boardLayout->addWidget(cbStartSource3, rowId, 3);
      QCheckBox * cbStartSource4 = new QCheckBox("LVDS", tab);
      boardLayout->addWidget(cbStartSource4, rowId, 4);

      //-------------------------------------
      rowId ++;
      QLabel * lbGlobalTrgSource = new QLabel("Global Trigger Source :", tab);
      lbGlobalTrgSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbGlobalTrgSource, rowId, 0);

      QCheckBox * cbGlbTrgSource1 = new QCheckBox("Trg-IN", tab);
      boardLayout->addWidget(cbGlbTrgSource1, rowId, 1);
      QCheckBox * cbGlbTrgSource2 = new QCheckBox("SwTrg", tab);
      boardLayout->addWidget(cbGlbTrgSource2, rowId, 2);
      QCheckBox * cbGlbTrgSource3 = new QCheckBox("LVDS", tab);
      boardLayout->addWidget(cbGlbTrgSource3, rowId, 3);
      QCheckBox * cbGlbTrgSource4 = new QCheckBox("GPIO", tab);
      boardLayout->addWidget(cbGlbTrgSource4, rowId, 4);
      QCheckBox * cbGlbTrgSource5 = new QCheckBox("TestPulse", tab);
      boardLayout->addWidget(cbGlbTrgSource5, rowId, 5);

      //-------------------------------------
      rowId ++;
      QLabel * lbTrgOutMode = new QLabel("Trg-OUT Mode :", tab);
      lbTrgOutMode->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbTrgOutMode, rowId, 0);

      QComboBox * comTrgOut = new QComboBox(tab);
      boardLayout->addWidget(comTrgOut, rowId, 1, 1, 2);
      comTrgOut->addItem("Disabled");
      comTrgOut->addItem("TRG-IN");
      comTrgOut->addItem("SwTrg");
      comTrgOut->addItem("LVDS");
      comTrgOut->addItem("Run");
      comTrgOut->addItem("TestPulse");
      comTrgOut->addItem("Busy");
      comTrgOut->addItem("Fixed 0");
      comTrgOut->addItem("Fixed 1");
      comTrgOut->addItem("SyncIn signal");
      comTrgOut->addItem("S-IN signal");
      comTrgOut->addItem("GPIO signal");
      comTrgOut->addItem("Accepted triggers");
      comTrgOut->addItem("Trigger Clock signal");

      //-------------------------------------
      rowId ++;
      QLabel * lbGPIOMode = new QLabel("GPIO Mode :", tab);
      lbGPIOMode->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbGPIOMode, rowId, 0);

      QComboBox * comGPIO = new QComboBox(tab);
      boardLayout->addWidget(comGPIO, rowId, 1, 1, 2);
      comGPIO->addItem("Disabled");
      comGPIO->addItem("TRG-IN");
      comGPIO->addItem("S-IN");
      comGPIO->addItem("LVDS");
      comGPIO->addItem("SwTrg");
      comGPIO->addItem("Run");
      comGPIO->addItem("TestPulse");
      comGPIO->addItem("Busy");
      comGPIO->addItem("Fixed 0");
      comGPIO->addItem("Fixed 1");

      //-------------------------------------
      rowId ++;
      QLabel * lbBusyInSource = new QLabel("Busy In Source :", tab);
      lbBusyInSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbBusyInSource, rowId, 0);

      QComboBox * comBusyIn = new QComboBox(tab);
      boardLayout->addWidget(comBusyIn, rowId, 1, 1, 2);
      comBusyIn->addItem("Disabled");
      comBusyIn->addItem("GPIO");
      comBusyIn->addItem("LVDS");
      comBusyIn->addItem("LVDS");

      //-------------------------------------
      rowId ++;
      QLabel * lbSyncOutMode = new QLabel("Sync Out mode :", tab);
      lbSyncOutMode->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbSyncOutMode, rowId, 0);

      QComboBox * comSyncOut = new QComboBox(tab);
      boardLayout->addWidget(comSyncOut, rowId, 1, 1, 2);
      comSyncOut->addItem("Disabled");
      comSyncOut->addItem("SyncIn");
      comSyncOut->addItem("TestPulse");
      comSyncOut->addItem("Internal 62.5 MHz Clock");
      comSyncOut->addItem("Run");

      //-------------------------------------
      rowId ++;
      QLabel * lbBoardVetoSource = new QLabel("Board Veto Source :", tab);
      lbBoardVetoSource->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbBoardVetoSource, rowId, 0);

      QComboBox * comBoardVetoSource = new QComboBox(tab);
      boardLayout->addWidget(comBoardVetoSource, rowId, 1, 1, 2);
      comBoardVetoSource->addItem("Disabled");
      comBoardVetoSource->addItem("S-In");
      comBoardVetoSource->addItem("LVSD");
      comBoardVetoSource->addItem("GPIO");

      QLabel * lbBdVetoWidth = new QLabel("Board Veto Width [ns] : ", tab);
      lbBdVetoWidth->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbBdVetoWidth, rowId, 3, 1, 2);

      QSpinBox * sbBdVetoWidth = new QSpinBox(tab); // may be QDoubleSpinBox
      sbBdVetoWidth->setMinimum(0);
      sbBdVetoWidth->setSingleStep(20);
      boardLayout->addWidget(sbBdVetoWidth, rowId, 5);

      QComboBox * comBdVetoPolarity = new QComboBox(tab);
      boardLayout->addWidget(comBdVetoPolarity, rowId, 6);
      comBdVetoPolarity->addItem("High");
      comBdVetoPolarity->addItem("Low");
      
      //-------------------------------------
      rowId ++;
      QLabel * lbRunDelay = new QLabel("Run Delay [ns] :", tab);
      lbRunDelay->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbRunDelay, rowId, 0);

      QSpinBox * sbRunDelay = new QSpinBox(tab);
      sbRunDelay->setMinimum(0);
      sbRunDelay->setMaximum(524280);
      sbRunDelay->setSingleStep(20);
      boardLayout->addWidget(sbRunDelay, rowId, 1);

      //-------------------------------------
      rowId ++;
      QLabel * lbAutoDisarmAcq = new QLabel("Auto disarm ACQ :", tab);
      lbAutoDisarmAcq->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbAutoDisarmAcq, rowId, 0);

      QComboBox * comAutoDisarmAcq = new QComboBox(tab);
      boardLayout->addWidget(comAutoDisarmAcq, rowId, 1);
      comAutoDisarmAcq->addItem("Enabled");
      comAutoDisarmAcq->addItem("Disabled");
  
      //-------------------------------------
      rowId ++;
      QLabel * lbStatEvents = new QLabel("Stat. Event :", tab);
      lbStatEvents->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbStatEvents, rowId, 0);

      QComboBox * comAStatEvents = new QComboBox(tab);
      boardLayout->addWidget(comAStatEvents, rowId, 1);
      comAStatEvents->addItem("Enabled");
      comAStatEvents->addItem("Disabled");

      //-------------------------------------
      rowId ++;
      QLabel * lbClockOutDelay = new QLabel("Clock Out Delay [ps] :", tab);
      lbClockOutDelay->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbClockOutDelay, rowId, 0);

      QDoubleSpinBox * dsbClockOutDelay = new QDoubleSpinBox(tab);
      dsbClockOutDelay->setMinimum(-1888.888);
      dsbClockOutDelay->setMaximum(1888.888);
      dsbClockOutDelay->setValue(0);
      boardLayout->addWidget(dsbClockOutDelay, rowId, 1);

      //-------------------------------------
      rowId ++;
      QLabel * lbIOLevel = new QLabel("IO Level :", tab);
      lbIOLevel->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbIOLevel, rowId, 0);

      QComboBox * comIOLevel = new QComboBox(tab);
      boardLayout->addWidget(comIOLevel, rowId, 1, 1, 2);
      comIOLevel->addItem("NIM (0 = 0V, 1 = -0.8V)");
      comIOLevel->addItem("TTL (0 = 0V, 1 =  3.3V)");

      //-------------------------------------- Test pulse

    }


    {//------- Group channel settings
      QGroupBox * chBox = new QGroupBox("Channel Settings", tab); tabLayout->addWidget(chBox, 1, 1, 1, 1);
      QGridLayout * chLayout = new QGridLayout(chBox); //chBox->setLayout(chLayout);


      QSignalMapper * onOffMapper = new QSignalMapper(tab);
      connect(onOffMapper, &QSignalMapper::mappedInt, this, &DigiSettings::onChannelonOff); 
    
      QTabWidget * chTabWidget = new QTabWidget(tab); chLayout->addWidget(chTabWidget);
      
      {//.......... All Settings tab
        QWidget * tab_All = new QWidget(tab); 
        chTabWidget->addTab(tab_All, "All Settings");
        tab_All->setStyleSheet("background-color: #EEEEEE");

        QGridLayout * allLayout = new QGridLayout(tab_All);
        allLayout->setHorizontalSpacing(0);
        allLayout->setVerticalSpacing(0);

        unsigned short ch = digi->GetNChannels();

        cbCh[iDigi][ch] = new QCheckBox("On/Off", tab); allLayout->addWidget(cbCh[iDigi][ch], 0, 0);
        onOffMapper->setMapping(cbCh[iDigi][ch], (iDigi << 12) + ch);
        connect(cbCh[iDigi][ch], SIGNAL(clicked()), onOffMapper, SLOT(map()));

        QLabel * lbRL = new QLabel("Record Length [ns]", tab); allLayout->addWidget(lbRL, 1, 0);
        lbRL->setAlignment(Qt::AlignRight);
        sbRecordLength[ch] = new QSpinBox(tab); allLayout->addWidget(sbRecordLength[ch], 1, 1);

        QLabel * lbPT = new QLabel("Pre Trigger [ns]", tab); allLayout->addWidget(lbPT, 1, 2);
        lbPT->setAlignment(Qt::AlignRight);
        sbPreTrigger[ch] = new QSpinBox(tab); allLayout->addWidget(sbPreTrigger[ch], 1, 3);

      }

      
      {//.......... Ch On/Off
        QWidget * tab_onOff = new QWidget(tab); chTabWidget->addTab(tab_onOff, "On/Off");
        tab_onOff->setStyleSheet("background-color: #EEEEEE");

        QGridLayout * allLayout = new QGridLayout(tab_onOff); 
        allLayout->setHorizontalSpacing(0);
        allLayout->setVerticalSpacing(0);

        
        for( int ch = 0; ch < digi->GetNChannels(); ch++){
          cbCh[iDigi][ch] = new QCheckBox(QString::number(ch)); allLayout->addWidget(cbCh[iDigi][ch], ch/8, ch%8);
          cbCh[iDigi][ch]->setLayoutDirection(Qt::RightToLeft);

          onOffMapper->setMapping(cbCh[iDigi][ch], (iDigi << 12) + ch);
          connect(cbCh[iDigi][ch], SIGNAL(clicked()), onOffMapper, SLOT(map()));
        }

      }



      /*
      for( unsigned short rowID = 0; rowID < digi->GetNChannels() + 2; rowID++){

        //------ set Labels
        if( rowID == 1){
          unsigned short colID = 0;          

          QLabel * labCh = new QLabel("Ch", tab);
          labCh->setAlignment(Qt::AlignRight);
          chLayout->addWidget(labCh, rowID, colID);

          colID ++;
          QLabel * labOnOff = new QLabel("On", tab);
          chLayout->addWidget(labOnOff, rowID, colID);

          colID ++;
          QLabel * labEvtTrg = new QLabel("Event Trg.", tab);
          labEvtTrg->setAlignment(Qt::AlignCenter);
          chLayout->addWidget(labEvtTrg, rowID, colID);
          
          colID ++;
          QLabel * labWaveTrg = new QLabel("Wave Trg.", tab);
          labWaveTrg->setAlignment(Qt::AlignCenter);
          chLayout->addWidget(labWaveTrg, rowID, colID);
          
          colID ++;
          QLabel * labWaveSave = new QLabel("Wave Save", tab);
          labWaveSave->setAlignment(Qt::AlignCenter);
          chLayout->addWidget(labWaveSave, rowID, colID);

          colID ++;
          QLabel * labWaveSource = new QLabel("Wave Source", tab);
          labWaveSource->setAlignment(Qt::AlignCenter);
          chLayout->addWidget(labWaveSource, rowID, colID);

          colID ++;
          QLabel * labWaveRes = new QLabel("Wave Res.", tab);
          labWaveRes->setAlignment(Qt::AlignCenter);
          chLayout->addWidget(labWaveRes, rowID, colID);

          colID ++;
          QLabel * labWaveLength = new QLabel("Wave Length [ns]", tab);
          labWaveLength->setAlignment(Qt::AlignCenter);
          chLayout->addWidget(labWaveLength, rowID, colID);

          colID ++;
          QLabel * labPreTrigger = new QLabel("Pre Trigger [ns]", tab);
          labPreTrigger->setAlignment(Qt::AlignCenter);
          chLayout->addWidget(labPreTrigger, rowID, colID);

          colID ++; QLabel * labAnaProbe0 = new QLabel("Ana. Probe 0", tab); labAnaProbe0->setAlignment(Qt::AlignCenter); chLayout->addWidget(labAnaProbe0, rowID, colID);
          colID ++; QLabel * labAnaProbe1 = new QLabel("Ana. Probe 1", tab); labAnaProbe1->setAlignment(Qt::AlignCenter); chLayout->addWidget(labAnaProbe1, rowID, colID);

          colID ++; QLabel * labDigProbe0 = new QLabel("Dig. Probe 0", tab); labDigProbe0->setAlignment(Qt::AlignCenter); chLayout->addWidget(labDigProbe0, rowID, colID);
          colID ++; QLabel * labDigProbe1 = new QLabel("Dig. Probe 1", tab); labDigProbe1->setAlignment(Qt::AlignCenter); chLayout->addWidget(labDigProbe1, rowID, colID);
          colID ++; QLabel * labDigProbe2 = new QLabel("Dig. Probe 2", tab); labDigProbe2->setAlignment(Qt::AlignCenter); chLayout->addWidget(labDigProbe2, rowID, colID);
          colID ++; QLabel * labDigProbe3 = new QLabel("Dig. Probe 3", tab); labDigProbe3->setAlignment(Qt::AlignCenter); chLayout->addWidget(labDigProbe3, rowID, colID);

          colID ++;
          QLabel * labChVetoSrc = new QLabel("Veto Source", tab);
          labChVetoSrc->setAlignment(Qt::AlignCenter);
          chLayout->addWidget(labChVetoSrc, rowID, colID);

          colID ++;
          QLabel * labChADCVetoWidth = new QLabel("Veto Width [ns]", tab);
          labChADCVetoWidth->setAlignment(Qt::AlignCenter);
          chLayout->addWidget(labChADCVetoWidth, rowID, colID);

        }

        //------ set all channel
        if( rowID == 0 || rowID >= 2){

          unsigned int ch = (rowID == 0 ? 64 : rowID-2);
          unsigned short colID = 0;    

          QLabel * labCh = new QLabel(rowID == 0 ? "All" : QString::number(ch), tab);
          labCh->setAlignment(Qt::AlignRight);
          chLayout->addWidget(labCh, rowID, colID);
      
          colID ++;
          cbCh[ch] = new QCheckBox(tab);
          chLayout->addWidget(cbCh[ch], rowID, colID);
          onOffMapper->setMapping(cbCh[ch], ch);
          connect(cbCh[ch], SIGNAL(clicked()), onOffMapper, SLOT(map()));

          colID ++;
          cmbEvtTrigger[ch] = new QComboBox(tab);
          cmbEvtTrigger[ch]->addItem("Disable");
          cmbEvtTrigger[ch]->addItem("SWTrigger");
          cmbEvtTrigger[ch]->addItem("ChSelfTrigger");
          cmbEvtTrigger[ch]->addItem("Ch64Trigger");
          cmbEvtTrigger[ch]->addItem("TRGIN");
          cmbEvtTrigger[ch]->addItem("Global");
          cmbEvtTrigger[ch]->addItem("ITLA");
          cmbEvtTrigger[ch]->addItem("ITLB");
          chLayout->addWidget(cmbEvtTrigger[ch], rowID, colID);

          colID ++;
          cmbWaveTrigger[ch] = new QComboBox(tab);
          cmbWaveTrigger[ch]->addItem("Disable");
          cmbWaveTrigger[ch]->addItem("SWTrigger");
          cmbWaveTrigger[ch]->addItem("ChSelfTrigger");
          cmbWaveTrigger[ch]->addItem("Ch64Trigger");
          cmbWaveTrigger[ch]->addItem("TRGIN");
          cmbWaveTrigger[ch]->addItem("ADC Over Sat.");
          cmbWaveTrigger[ch]->addItem("ADC under Sat.");
          cmbWaveTrigger[ch]->addItem("Global");
          cmbWaveTrigger[ch]->addItem("ITLA");
          cmbWaveTrigger[ch]->addItem("ITLB");
          cmbWaveTrigger[ch]->addItem("Ext.Inhibit");
          chLayout->addWidget(cmbWaveTrigger[ch], rowID, colID);
          
          colID ++;
          cmbWaveSave[ch] = new QComboBox(tab);
          cmbWaveSave[ch]->addItem("Always");
          cmbWaveSave[ch]->addItem("on Request");
          chLayout->addWidget(cmbWaveSave[ch], rowID, colID);
          
          colID ++;
          cmbWaveSource[ch] = new QComboBox(tab);
          cmbWaveSource[ch]->addItem("ADC");
          cmbWaveSource[ch]->addItem("Test Toggle");
          cmbWaveSource[ch]->addItem("Test Ramp");
          cmbWaveSource[ch]->addItem("Test Sin wave");
          cmbWaveSource[ch]->addItem("Ramp");
          cmbWaveSource[ch]->addItem("Square Wave");
          chLayout->addWidget(cmbWaveSource[ch], rowID, colID);

          colID ++;
          cmbWaveRes[ch] = new QComboBox(tab);
          cmbWaveRes[ch]->addItem(" 8 ns");
          cmbWaveRes[ch]->addItem("16 ns");
          cmbWaveRes[ch]->addItem("32 ns");
          cmbWaveRes[ch]->addItem("64 ns");
          chLayout->addWidget(cmbWaveRes[ch], rowID, colID);

          colID ++;
          sbRecordLength[ch] = new QSpinBox(tab);
          sbRecordLength[ch]->setMinimum(32);
          sbRecordLength[ch]->setMaximum(64800);
          sbRecordLength[ch]->setSingleStep(8);
          chLayout->addWidget(sbRecordLength[ch], rowID, colID);

          colID ++;
          sbPreTrigger[ch] = new QSpinBox(tab);
          sbPreTrigger[ch]->setMinimum(32);
          sbPreTrigger[ch]->setMaximum(32000);
          sbPreTrigger[ch]->setSingleStep(8);
          chLayout->addWidget(sbPreTrigger[ch], rowID, colID);

          colID ++;
          cmbAnaProbe0[ch] = new QComboBox(tab);
          cmbAnaProbe0[ch]->addItem("ADC Input");
          cmbAnaProbe0[ch]->addItem("Time Filter");
          cmbAnaProbe0[ch]->addItem("Trapazoid");
          cmbAnaProbe0[ch]->addItem("Trap. Baseline");
          cmbAnaProbe0[ch]->addItem("Trap. - Baseline");
          chLayout->addWidget(cmbAnaProbe0[ch], rowID, colID);

          colID ++;
          cmbAnaProbe1[ch] = new QComboBox(tab);
          cmbAnaProbe1[ch]->addItem("ADC Input");
          cmbAnaProbe1[ch]->addItem("Time Filter");
          cmbAnaProbe1[ch]->addItem("Trapazoid");
          cmbAnaProbe1[ch]->addItem("Trap. Baseline");
          cmbAnaProbe1[ch]->addItem("Trap. - Baseline");
          chLayout->addWidget(cmbAnaProbe1[ch], rowID, colID);

          colID ++;
          cmbDigProbe0[ch] = new QComboBox(tab);
          cmbDigProbe0[ch]->addItem("Trigger");
          cmbDigProbe0[ch]->addItem("Time Filter Armed");
          cmbDigProbe0[ch]->addItem("ReTrigger Guard");
          cmbDigProbe0[ch]->addItem("Trap. basline Freeze");
          cmbDigProbe0[ch]->addItem("Peaking");
          cmbDigProbe0[ch]->addItem("Peak Ready");
          cmbDigProbe0[ch]->addItem("Pile-up Guard");
          cmbDigProbe0[ch]->addItem("ADC Saturate");
          cmbDigProbe0[ch]->addItem("ADC Sat. Protection");
          cmbDigProbe0[ch]->addItem("Post Sat. Event");
          cmbDigProbe0[ch]->addItem("Trap. Saturate");
          cmbDigProbe0[ch]->addItem("ACQ Inhibit");
          chLayout->addWidget(cmbDigProbe0[ch], rowID, colID);
          colID ++;
          cmbDigProbe1[ch] = new QComboBox(tab);
          cmbDigProbe1[ch]->addItem("Trigger");
          cmbDigProbe1[ch]->addItem("Time Filter Armed");
          cmbDigProbe1[ch]->addItem("ReTrigger Guard");
          cmbDigProbe1[ch]->addItem("Trap. basline Freeze");
          cmbDigProbe1[ch]->addItem("Peaking");
          cmbDigProbe1[ch]->addItem("Peak Ready");
          cmbDigProbe1[ch]->addItem("Pile-up Guard");
          cmbDigProbe1[ch]->addItem("ADC Saturate");
          cmbDigProbe1[ch]->addItem("ADC Sat. Protection");
          cmbDigProbe1[ch]->addItem("Post Sat. Event");
          cmbDigProbe1[ch]->addItem("Trap. Saturate");
          cmbDigProbe1[ch]->addItem("ACQ Inhibit");
          chLayout->addWidget(cmbDigProbe1[ch], rowID, colID);
          colID ++;
          cmbDigProbe2[ch] = new QComboBox(tab);
          cmbDigProbe2[ch]->addItem("Trigger");
          cmbDigProbe2[ch]->addItem("Time Filter Armed");
          cmbDigProbe2[ch]->addItem("ReTrigger Guard");
          cmbDigProbe2[ch]->addItem("Trap. basline Freeze");
          cmbDigProbe2[ch]->addItem("Peaking");
          cmbDigProbe2[ch]->addItem("Peak Ready");
          cmbDigProbe2[ch]->addItem("Pile-up Guard");
          cmbDigProbe2[ch]->addItem("ADC Saturate");
          cmbDigProbe2[ch]->addItem("ADC Sat. Protection");
          cmbDigProbe2[ch]->addItem("Post Sat. Event");
          cmbDigProbe2[ch]->addItem("Trap. Saturate");
          cmbDigProbe2[ch]->addItem("ACQ Inhibit");
          chLayout->addWidget(cmbDigProbe2[ch], rowID, colID);
          colID ++;
          cmbDigProbe3[ch] = new QComboBox(tab);
          cmbDigProbe3[ch]->addItem("Trigger");
          cmbDigProbe3[ch]->addItem("Time Filter Armed");
          cmbDigProbe3[ch]->addItem("ReTrigger Guard");
          cmbDigProbe3[ch]->addItem("Trap. basline Freeze");
          cmbDigProbe3[ch]->addItem("Peaking");
          cmbDigProbe3[ch]->addItem("Peak Ready");
          cmbDigProbe3[ch]->addItem("Pile-up Guard");
          cmbDigProbe3[ch]->addItem("ADC Saturate");
          cmbDigProbe3[ch]->addItem("ADC Sat. Protection");
          cmbDigProbe3[ch]->addItem("Post Sat. Event");
          cmbDigProbe3[ch]->addItem("Trap. Saturate");
          cmbDigProbe3[ch]->addItem("ACQ Inhibit");
          chLayout->addWidget(cmbDigProbe3[ch], rowID, colID);


          colID ++;
          cmbChVetoSrc[ch] = new QComboBox(tab);
          cmbChVetoSrc[ch]->addItem("Disable");
          cmbChVetoSrc[ch]->addItem("BoardVeto");
          cmbChVetoSrc[ch]->addItem("ADC Over Sat.");
          cmbChVetoSrc[ch]->addItem("ADC under Sat.");
          chLayout->addWidget(cmbChVetoSrc[ch], rowID, colID);

          colID ++;
          sbChADCVetoWidth[ch] = new QSpinBox(tab);
          sbChADCVetoWidth[ch]->setMinimum(0);
          sbChADCVetoWidth[ch]->setMaximum(524280);
          sbChADCVetoWidth[ch]->setSingleStep(20);
          chLayout->addWidget(sbChADCVetoWidth[ch], rowID, colID);

        }

      }*/ //-- end of ch loop;
    }

    {//------- Group trigger settings
      QGroupBox * triggerBox = new QGroupBox("Trigger Map", tab);
      QGridLayout * triggerLayout = new QGridLayout(triggerBox);
      //triggerBox->setLayout(triggerLayout);
      tabLayout->addWidget(triggerBox, 2, 0);
      
      triggerLayout->setHorizontalSpacing(0);
      triggerLayout->setVerticalSpacing(0);

      QLabel * instr = new QLabel("Reading: Column (C) represents a trigger channel for Row (R) channel.\nFor example, R3C1 = ch-3 trigger source is ch-1.\n", tab);
      triggerLayout->addWidget(instr, 0, 0, 1, 64+15);

      QSignalMapper * triggerMapper = new QSignalMapper(tab);
      connect(triggerMapper, &QSignalMapper::mappedInt, this, &DigiSettings::onTriggerClick);

      int rowID = 1;
      int colID = 0;
      for(int i = 0; i < digi->GetNChannels(); i++){
        colID = 0;
        for(int j = 0; j < digi->GetNChannels(); j++){
          
          bn[i][j] = new QPushButton(tab);
          bn[i][j]->setFixedSize(QSize(10,10));
          bnClickStatus[i][j] = false;

          if( i%4 != 0 && j == (i/4)*4) {
            bn[i][j]->setStyleSheet("background-color: red;");
            bnClickStatus[i][j] = true;
          }
          triggerLayout->addWidget(bn[i][j], rowID, colID);

          triggerMapper->setMapping(bn[i][j], (iDigi << 12) + (i << 8) + j);
          connect(bn[i][j], SIGNAL(clicked()), triggerMapper, SLOT(map()));

          colID ++;

          if( j%4 == 3 && j!= digi->GetNChannels() - 1){
            QFrame * vSeparator = new QFrame(tab);
            vSeparator->setFrameShape(QFrame::VLine);
            triggerLayout->addWidget(vSeparator, rowID, colID);
            colID++;
          }
        }

        rowID++;

        if( i%4 == 3 && i != digi->GetNChannels() - 1){
          QFrame * hSeparator = new QFrame(tab);
          hSeparator->setFrameShape(QFrame::HLine);
          triggerLayout->addWidget(hSeparator, rowID, 0, 1, digi->GetNChannels() + 15);
          rowID++;
        }


      }
    }


  } //=== end of tab


}

DigiSettings::~DigiSettings(){

  printf("%s\n", __func__);

  for( int iDig = 0; iDig < nDigi; iDig ++){
    for( int i =0 ; i < MaxNumberOfChannel; i++){
      if( cbCh[iDig][i] != NULL) delete cbCh[iDig][i];
    }
  }

}