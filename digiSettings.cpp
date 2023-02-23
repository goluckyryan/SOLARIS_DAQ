#include "digiSettings.h"

#include <QLabel>

DigiSettings::DigiSettings(Digitizer2Gen ** digi, unsigned short nDigi, QWidget * parent) : QWidget(parent){

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

  //@========================== Tab for each digitizer
  for(unsigned short iDigi = 0; iDigi < this->nDigi; iDigi++){

    QWidget * tab = new QWidget(tabWidget);
    QScrollArea * scrollArea = new QScrollArea(this); scrollArea->setWidget(tab);
    scrollArea->setWidgetResizable(true);
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    tabWidget->addTab(scrollArea, "Digi-" + QString::number(digi[iDigi]->GetSerialNumber()));
    
    QGridLayout *tabLayout = new QGridLayout(tab); tab->setLayout(tabLayout);

    {//^====================== Group of Digitizer Info
      QGroupBox * infoBox = new QGroupBox("Board Info", tab);
      QGridLayout * infoLayout = new QGridLayout(infoBox);
      tabLayout->addWidget(infoBox, 0, 0);
      
      const unsigned short nRow = 4;
      for( unsigned short j = 0; j < (unsigned short) info.size(); j++){
        QLabel * lab = new QLabel(QString::fromStdString(info[j][0]), tab);
        lab->setAlignment(Qt::AlignRight);
        QLineEdit * txt = new QLineEdit(tab);
        txt->setReadOnly(true);
        txt->setText(QString::fromStdString(digi[iDigi]->ReadValue(info[j][1].c_str())));
        infoLayout->addWidget(lab, j%nRow, 2*(j/nRow));
        infoLayout->addWidget(txt, j%nRow, 2*(j/nRow) +1);
      }
    }

    {//^====================== Group Board status
      QGroupBox * statusBox = new QGroupBox("Board Status", tab);
      QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
      sizePolicy.setHorizontalStretch(0);
      sizePolicy.setVerticalStretch(0);
      statusBox->setSizePolicy(sizePolicy);
      QGridLayout * statusLayout = new QGridLayout(statusBox);
      statusLayout->setHorizontalSpacing(0);

      tabLayout->addWidget(statusBox, 0, 1);

      //------- LED Status
      QLabel * lbLED = new QLabel("LED status : ");
      lbLED->setAlignment(Qt::AlignRight | Qt::AlignCenter);
      statusLayout->addWidget(lbLED, 0, 0);

      QPushButton ** LEDStatus = new QPushButton *[19];      
      for( int i = 0; i < 19; i++){
        LEDStatus[i] = new QPushButton(tab);
        LEDStatus[i]->setEnabled(false);
        LEDStatus[i]->setFixedSize(QSize(30,30));
        statusLayout->addWidget(LEDStatus[i], 0, 1 + i);
      }

      //------- ACD Status
      QLabel * lbACQ = new QLabel("ACQ status : ");
      lbACQ->setAlignment(Qt::AlignRight | Qt::AlignCenter);
      statusLayout->addWidget(lbACQ, 1, 0);

      QPushButton ** ACQStatus = new QPushButton *[7];      
      for( int i = 0; i < 7; i++){
        ACQStatus[i] = new QPushButton(tab);
        ACQStatus[i]->setEnabled(false);
        ACQStatus[i]->setFixedSize(QSize(30,30));
        statusLayout->addWidget(ACQStatus[i], 1, 1 + i);
      }

      //------- Temperatures
      QLabel * lbTemp = new QLabel("ADC Temperature [C] : ");
      lbTemp->setAlignment(Qt::AlignRight | Qt::AlignCenter);
      statusLayout->addWidget(lbTemp, 2, 0);

      const int nTemp = (int) DIGIPARA::DIG::TempSensADC.size();
      QLineEdit ** leTemp = new QLineEdit* [nTemp]; 
      for( int i = 0; i < nTemp; i++){
        leTemp[i] = new QLineEdit(tab);
        leTemp[i]->setEnabled(false);
        statusLayout->addWidget(leTemp[i], 2, 1 + 2*i, 1, 2);
      }

    }

    {//^====================== Group Board settings
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

      QPushButton * bnSaveSettings = new QPushButton("Save Settings", tab);
      boardLayout->addWidget(bnSaveSettings, rowId, 4, 1, 2);
      
      QPushButton * bnLoadSettings = new QPushButton("Load Settings", tab);
      boardLayout->addWidget(bnLoadSettings, rowId, 6, 1, 2);

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
      QLabel * lbAutoDisarmAcq = new QLabel("Auto disarm ACQ :", tab);
      lbAutoDisarmAcq->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbAutoDisarmAcq, rowId, 4, 1, 2);

      QComboBox * comAutoDisarmAcq = new QComboBox(tab);
      boardLayout->addWidget(comAutoDisarmAcq, rowId, 6);
      comAutoDisarmAcq->addItem("Enabled");
      comAutoDisarmAcq->addItem("Disabled");

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
      QLabel * lbStatEvents = new QLabel("Stat. Event :", tab);
      lbStatEvents->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbStatEvents, rowId, 4, 1, 2);

      QComboBox * comAStatEvents = new QComboBox(tab);
      boardLayout->addWidget(comAStatEvents, rowId, 6);
      comAStatEvents->addItem("Enabled");
      comAStatEvents->addItem("Disabled");

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
      QLabel * lbClockOutDelay = new QLabel("Clock Out Delay [ps] :", tab);
      lbClockOutDelay->setAlignment(Qt::AlignRight);
      boardLayout->addWidget(lbClockOutDelay, rowId, 3, 1, 2);

      QDoubleSpinBox * dsbClockOutDelay = new QDoubleSpinBox(tab);
      dsbClockOutDelay->setMinimum(-1888.888);
      dsbClockOutDelay->setMaximum(1888.888);
      dsbClockOutDelay->setValue(0);
      boardLayout->addWidget(dsbClockOutDelay, rowId, 5);

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


    {//^====================== Group channel settings
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

        unsigned short ch = digi[iDigi]->GetNChannels();

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

        
        for( int ch = 0; ch < digi[iDigi]->GetNChannels(); ch++){
          cbCh[iDigi][ch] = new QCheckBox(QString::number(ch)); allLayout->addWidget(cbCh[iDigi][ch], ch/8, ch%8);
          cbCh[iDigi][ch]->setLayoutDirection(Qt::RightToLeft);

          onOffMapper->setMapping(cbCh[iDigi][ch], (iDigi << 12) + ch);
          connect(cbCh[iDigi][ch], SIGNAL(clicked()), onOffMapper, SLOT(map()));
        }

      }

    }

    {//^====================== Group trigger settings
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
      for(int i = 0; i < digi[iDigi]->GetNChannels(); i++){
        colID = 0;
        for(int j = 0; j < digi[iDigi]->GetNChannels(); j++){
          
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

          if( j%4 == 3 && j!= digi[iDigi]->GetNChannels() - 1){
            QFrame * vSeparator = new QFrame(tab);
            vSeparator->setFrameShape(QFrame::VLine);
            triggerLayout->addWidget(vSeparator, rowID, colID);
            colID++;
          }
        }

        rowID++;

        if( i%4 == 3 && i != digi[iDigi]->GetNChannels() - 1){
          QFrame * hSeparator = new QFrame(tab);
          hSeparator->setFrameShape(QFrame::HLine);
          triggerLayout->addWidget(hSeparator, rowID, 0, 1, digi[iDigi]->GetNChannels() + 15);
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