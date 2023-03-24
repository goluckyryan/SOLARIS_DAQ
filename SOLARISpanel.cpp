#include "SOLARISpanel.h"

#include <QFile>
#include <QLabel>
#include <QSet>
#include <QList>
#include <QFrame>

#define NCOL 10 // number of column

std::vector<Reg> SettingItems = {PHA::CH::TriggerThreshold, PHA::CH::DC_Offset};
const std::vector<QString> arrayLabel = {"e", "xf", "xn"};

SOLARISpanel::SOLARISpanel(Digitizer2Gen **digi, unsigned short nDigi, 
                          std::vector<std::vector<int>> mapping, 
                          QStringList detType, 
                          std::vector<int> detMaxID, 
                          QWidget *parent) : QWidget(parent){

  setWindowTitle("SOLARIS Settings");
  setGeometry(0, 0, 1350, 800);

  this->digi = digi;
  this->nDigi = nDigi;
  this->mapping = mapping;
  this->detType = detType;
  this->detMaxID = detMaxID;

  //Check number of detector type; Array 0-199, Recoil 200-299, other 300-
  int nDetType = detType.size();
  for( int k = 0 ; k < nDetType; k++) nDet.push_back(0);

  QList<QList<int>> detIDListTemp; // store { detID,  (Digi << 8 ) + ch}, making mapping into 1-D array to be consolidated

  printf("################################# \n");
  for( int i = 0; i < (int) mapping.size() ; i++){
    for( int j = 0; j < (int) mapping[i].size(); j++ ){
      printf("%3d,", mapping[i][j]);
      QList<int> haha ;
      haha << mapping[i][j] << ((i << 8 ) + j);
      if( mapping[i][j] >= 0 ) detIDListTemp <<  haha;
      if( j % 16 == 15 ) printf("\n");
    }
    printf("------------------ \n");
  }

  //----- consolidate detIDList;
  detIDList.clear();
  bool repeated = false;
  for( int i = 0; i < detIDListTemp.size(); i++ ){
    repeated = false;
    if( detIDList.size() == 0 ){
      detIDList << detIDListTemp[i];

      for( int k = 0 ; k < nDetType; k++){
        int lowID = (k == 0 ? 0 : detMaxID[k-1]);     
        if( lowID <= detIDListTemp[i][0] && detIDListTemp[i][0] < detMaxID[k] ) nDet[k] ++ ;
      }
      continue;
    }
    for( int j = 0; j < detIDList.size() ; j++){
      if( detIDList[j][0] == detIDListTemp[i][0] ) {
        repeated = true;
        detIDList[j] << detIDListTemp[i][1];
        break;
      }
    }
    if( !repeated ) {
      detIDList << detIDListTemp[i];
      for( int k = 0 ; k < nDetType; k++){
        int lowID = (k == 0 ? 0 : detMaxID[k-1]);     
        if( lowID <= detIDListTemp[i][0] && detIDListTemp[i][0] < detMaxID[k] ) nDet[k] ++ ;
      }
    }
  }

  //---- sort detIDList;
  std::sort(detIDList.begin(), detIDList.end(), [](const QList<int>& a, const QList<int>& b) {
    return a.at(0) < b.at(0);
  });

  //------------- display detector summary
  //qDebug() << detIDList;
  printf("---------- num. of det. Type : %d\n", nDetType);
  for( int i =0; i < nDetType; i ++ ) {
    printf(" Type-%d (%6s) : %3d det.  (%3d - %3d)\n", i, detType[i].toStdString().c_str(), nDet[i], (i==0 ? 0 : detMaxID[i-1]), detMaxID[i]-1);
  }

  //---------- Set Panel
  QGridLayout * mainLayout = new QGridLayout(this); this->setLayout(mainLayout);

  ///=================================
  int rowIndex = 0;
  QPushButton * bnRefresh = new QPushButton("Refresh Settings", this);
  connect(bnRefresh, &QPushButton::clicked, this, &SOLARISpanel::RefreshSettings );
  mainLayout->addWidget(bnRefresh, rowIndex, 0);

  QPushButton * bnSaveSetting = new QPushButton("Save Settings", this);
  connect(bnSaveSetting, &QPushButton::clicked, this, &SOLARISpanel::SaveSettings);
  mainLayout->addWidget(bnSaveSetting, rowIndex, 1);  
  
  QPushButton * bnLoadSetting = new QPushButton("Load Settings", this);
  connect(bnLoadSetting, &QPushButton::clicked, this, &SOLARISpanel::LoadSettings);
  mainLayout->addWidget(bnLoadSetting, rowIndex, 2);

  QLabel * lbCoinTime = new QLabel("Coin. Time (all ch.) [ns]", this);
  lbCoinTime->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  mainLayout->addWidget(lbCoinTime, rowIndex, 3);

  sbCoinTime = new RSpinBox(this);
  sbCoinTime->setMinimum(-1);
  sbCoinTime->setMaximum(atof(PHA::CH::CoincidenceLength.GetAnswers()[1].first.c_str()));
  sbCoinTime->setSingleStep(atof(PHA::CH::CoincidenceLength.GetAnswers()[2].first.c_str()));
  sbCoinTime->setDecimals(0);
  sbCoinTime->SetToolTip(atof(PHA::CH::CoincidenceLength.GetAnswers()[1].first.c_str()));
  mainLayout->addWidget(sbCoinTime, rowIndex, 4);

  connect(sbCoinTime, &RSpinBox::valueChanged, this, [=](){
    if( !enableSignalSlot ) return;
    sbCoinTime->setStyleSheet("color:blue;");
  });

  connect(sbCoinTime, &RSpinBox::returnPressed, this, [=](){
    if( !enableSignalSlot ) return;
    //printf("%s %d  %d \n", para.GetPara().c_str(), index, spb->value());
    if( sbCoinTime->decimals() == 0 && sbCoinTime->singleStep() != 1) {
      double step = sbCoinTime->singleStep();
      double value = sbCoinTime->value();
      sbCoinTime->setValue( (std::round(value/step) * step) );
    }

    for(int i = 0; i < (int) mapping.size(); i ++){
      if( i >= nDigi || digi[i]->IsDummy() || !digi[i]->IsConnected() ) return;
      QString msg;
      msg = QString::fromStdString(PHA::CH::CoincidenceLength.GetPara()) + "|DIG:"+ QString::number(digi[i]->GetSerialNumber());
      msg += ",CH:All = " + QString::number(sbCoinTime->value());
      if( digi[i]->WriteValue(PHA::CH::CoincidenceLength, std::to_string(sbCoinTime->value()))){
        SendLogMsg(msg + "|OK.");
        sbCoinTime->setStyleSheet("");
      }else{
        SendLogMsg(msg + "|Fail.");
        sbCoinTime->setStyleSheet("color:red;");
      }
    }
    UpdatePanelFromMemory();
  });

  ///=================================
  rowIndex ++;
  QLabel * info = new QLabel("Only simple trigger is avalible. For complex trigger scheme, please use the setting panel.", this);
  mainLayout->addWidget(info, rowIndex, 0, 1, 4);

  ///=================================
  rowIndex ++;
  QTabWidget * tabWidget = new QTabWidget(this); mainLayout->addWidget(tabWidget, rowIndex, 0, 1, 5);
  for( int detTypeID = 0; detTypeID < nDetType; detTypeID ++ ){

    QTabWidget * tabSetting = new QTabWidget(tabWidget);
    tabWidget->addTab(tabSetting, detType[detTypeID]);

    for( int SettingID = 0; SettingID < (int) SettingItems.size(); SettingID ++){

      QScrollArea * scrollArea = new QScrollArea(this);
      scrollArea->setWidgetResizable(true);
      scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
      tabSetting->addTab(scrollArea,  QString::fromStdString(SettingItems[SettingID]));

      QWidget * tab = new QWidget(tabSetting);
      scrollArea->setWidget(tab);
      
      QGridLayout * layout = new QGridLayout(tab);
      layout->setAlignment(Qt::AlignLeft|Qt::AlignTop);
      layout->setSpacing(0);

      //TODO======= coincident Time window

      //TODO======= check all
      chkAll = new QCheckBox("Set for all", tab);
      layout->addWidget(chkAll, 0, 0);

      if( SettingItems[SettingID].GetPara() == PHA::CH::TriggerThreshold.GetPara() ){
        chkAlle = new QCheckBox("Set for all e", tab);
        layout->addWidget(chkAlle, 0, 1);

        chkAllxf = new QCheckBox("Set for all xf", tab);
        layout->addWidget(chkAllxf, 0, 2);

        chkAllxn = new QCheckBox("Set for all xn", tab);
        layout->addWidget(chkAllxn, 0, 3);

      }

      QFrame *line = new QFrame(tab);
      line->setFrameShape(QFrame::HLine);
      line->setFrameShadow(QFrame::Sunken);
      line->setFixedHeight(10);
      layout->addWidget(line, 1, 0, 1, NCOL);


      //range of detID 
      int lowID = (detTypeID == 0 ? 0 : detMaxID[detTypeID-1]);

      for(int i = 0; i < detIDList.size(); i++){

        if( detIDList[i][0] >= detMaxID[detTypeID] || lowID > detIDList[i][0] ) continue;
        CreateDetGroup(SettingID, detIDList[i], layout, i/NCOL +  2, i%NCOL);

      }

    }
  }

}

SOLARISpanel::~SOLARISpanel(){

}

//^######################################################################
int SOLARISpanel::FindDetTypID(QList<int> detIDListElement){
  for( int i = 0; i < (int) detType.size(); i++){
    int lowID = (i == 0) ? 0 : detMaxID[i-1];
    if( lowID <= detIDListElement[0] && detIDListElement[0] < detMaxID[i]) {
      return i;
    }
  }
  return -1;
}

void SOLARISpanel::CreateDetGroup(int SettingID, QList<int> detID, QGridLayout * &layout, int row, int col){

  QGroupBox * groupbox = new QGroupBox("Det-" + QString::number(detID[0]), this);
  groupbox->setFixedWidth(130);
  groupbox->setAlignment(Qt::AlignCenter);
  QGridLayout * layout0 = new QGridLayout(groupbox);
  layout0->setSpacing(0);
  layout0->setAlignment(Qt::AlignLeft);

  //@======================================== SpinBox and Display
  bool isDisableDetector = false;
  for( int i = 1; i < (int) detID.size(); i ++){

    isDisableDetector = false;

    QLabel * lb  = new QLabel(arrayLabel[i-1], this);  
    layout0->addWidget(lb, 2*i, 0, 2, 1);

    int digiID = (detID[i] >> 8 );
    int chID = (detID[i] & 0xFF);

    chkOnOff[SettingID][digiID][chID] = new QCheckBox(this);
    layout0->addWidget(chkOnOff[SettingID][digiID][chID], 2*i, 1, 2, 1);

    leDisplay[SettingID][digiID][chID] = new QLineEdit(this);
    leDisplay[SettingID][digiID][chID]->setFixedWidth(70);
    layout0->addWidget(leDisplay[SettingID][digiID][chID], 2*i, 2);

    sbSetting[SettingID][digiID][chID] = new RSpinBox(this);
    if( digiID < nDigi ) sbSetting[SettingID][digiID][chID]->setToolTip( "Digi-" + QString::number(digi[digiID]->GetSerialNumber()) + ", Ch-" + QString::number(chID));
    sbSetting[SettingID][digiID][chID]->setToolTipDuration(-1);
    sbSetting[SettingID][digiID][chID]->setFixedWidth(70);

    sbSetting[SettingID][digiID][chID]->setMinimum(atoi(SettingItems[SettingID].GetAnswers()[0].first.c_str()));
    sbSetting[SettingID][digiID][chID]->setMaximum(atoi(SettingItems[SettingID].GetAnswers()[1].first.c_str()));
    sbSetting[SettingID][digiID][chID]->setSingleStep(atoi(SettingItems[SettingID].GetAnswers()[2].first.c_str()));
    sbSetting[SettingID][digiID][chID]->SetToolTip();

    layout0->addWidget(sbSetting[SettingID][digiID][chID], 2*i+1, 2);

    if( digiID >= nDigi || chID >= digi[digiID]->GetNChannels() ) {
      leDisplay[SettingID][digiID][chID]->setEnabled(false);
      sbSetting[SettingID][digiID][chID]->setEnabled(false);    
      chkOnOff[SettingID][digiID][chID]->setEnabled(false); 
      isDisableDetector = true;   
    }

    ///========================= for SpinBox
    RSpinBox * spb = sbSetting[SettingID][digiID][chID];
    const Reg para = SettingItems[SettingID];

    connect(spb, &RSpinBox::valueChanged, this, [=](){
      if( !enableSignalSlot ) return; 
      spb->setStyleSheet("color:blue;");
    });

    connect(spb, &RSpinBox::returnPressed, this, [=](){
      if( !enableSignalSlot ) return;
      //printf("%s %d  %d \n", para.GetPara().c_str(), index, spb->value());
      if( spb->decimals() == 0 && spb->singleStep() != 1) {
        double step = spb->singleStep();
        double value = spb->value();
        spb->setValue( (std::round(value/step) * step) );
      }
      QString msg;
      msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[digiID]->GetSerialNumber());
      if( para.GetType() == TYPE::CH ) msg += ",CH:" + QString::number(chID);
      msg += " = " + QString::number(spb->value());
      if( digi[digiID]->WriteValue(para, std::to_string(spb->value()), chID)){
        SendLogMsg(msg + "|OK.");
        spb->setStyleSheet("");
      }else{
        SendLogMsg(msg + "|Fail.");
        spb->setStyleSheet("color:red;");
      }
      UpdatePanelFromMemory();
      UpdateOtherPanels();
    });
    
    ///===================== for the OnOff CheckBox
    connect(chkOnOff[SettingID][digiID][chID], &QCheckBox::stateChanged, this, [=](int state){
      if( !enableSignalSlot ) return; 

      digi[digiID]->WriteValue(PHA::CH::ChannelEnable, state ? "True" : "False", chID);

      enableSignalSlot = false;
      for( int i = 0; i < (int) detType.size(); i++){
        leDisplay[i][digiID][chID]->setEnabled(state);
        sbSetting[i][digiID][chID]->setEnabled(state);
        chkOnOff[i][digiID][chID]->setChecked(state);
      }
      enableSignalSlot = true;

      UpdatePanelFromMemory();
      UpdateOtherPanels();

    });

    layout0->setColumnStretch(0, 1);
    layout0->setColumnStretch(1, 1);
    layout0->setColumnStretch(2, 5);

  }

  //@======================================== Trigger Combox
  //=================== The Trigger depnds on 5 settings (at least)
  //   EventTriggerSource, WaveTriggerSource, CoincidentMask, AntiCoincidentMask
  // 1,  EventTriggerSource has 8 settings, ITLA, ITLB, GlobalTriggerSource, TRGIN, SWTrigger, ChSelfTrigger, Ch64Trigger, Disabled 
  // 2,  WaveTriggerSource  has 8 settings, always set to be equal EventTriggerSource
  // 3,  CoincidentMak      has 6 Settings, Disabled, Ch64Trigger, TRGIN, GloableTriggerSource, ITLA, ITLB
  // 4,  AntiCoincidentMask has 6 Settings, always disabled
  // 5,  ChannelTriggerMask is a 64-bit
  // 6,  CoincidenceLengthT in ns, set to be 100 ns. 

  int detTypeID = FindDetTypID(detID);
  if( SettingItems[SettingID].GetPara() == PHA::CH::TriggerThreshold.GetPara()){
    cbTrigger[detTypeID][detID[0]] = new RComboBox(this); 
    cbTrigger[detTypeID][detID[0]]->addItem("Self Trigger", "ChSelfTrigger");    /// no coincident
    cbTrigger[detTypeID][detID[0]]->addItem("Trigger e", 0x1); // Self-trigger and coincient Ch64Trigger
    cbTrigger[detTypeID][detID[0]]->addItem("Ext. Trigger", "TRGIN");  // with coincident with TRGIN.
    cbTrigger[detTypeID][detID[0]]->addItem("Disabled", "Disabled");  // no Trigger, no coincident, basically channel still alive, but no recording
    cbTrigger[detTypeID][detID[0]]->addItem("Others", -999); // other settings
    
    layout0->addWidget(cbTrigger[detTypeID][detID[0]], 8, 0, 1, 3);

    if( isDisableDetector ) cbTrigger[detTypeID][detID[0]]->setEnabled(false);
  
    connect(cbTrigger[detTypeID][detID[0]], &RComboBox::currentIndexChanged, this , [=](int index){
      if( !enableSignalSlot) return;

      for( int i = 1; i < detID.size(); i++){

        int digiID = (detID[i] >> 8 );
        int chID = (detID[i] & 0xFF);

        if( digi[digiID]->IsDummy() || !digi[digiID]->IsConnected() ) continue;

        digi[digiID]->WriteValue(PHA::CH::AntiCoincidenceMask, "Disabled", chID);

        switch(index){
          case 0 : { /// Self Trigger
            digi[digiID]->WriteValue(PHA::CH::EventTriggerSource, "ChSelfTrigger", chID);
            digi[digiID]->WriteValue(PHA::CH::WaveTriggerSource, "ChSelfTrigger", chID);
            digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Disabled", chID);
          }; break;
          case 1 : { // trigger by energy
            digi[digiID]->WriteValue(PHA::CH::EventTriggerSource, "ChSelfTrigger", chID);
            digi[digiID]->WriteValue(PHA::CH::WaveTriggerSource, "ChSelfTrigger", chID);

            if( i == 1 ){
              digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Disabled", chID);
            }else {
              digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Ch64Trigger", chID);
              digi[digiID]->WriteValue(PHA::CH::CoincidenceLength, "100", chID);

              //Form the trigger bit
              unsigned long mask = 1ULL << (detID[1] & 0xFF ); // trigger by energy
              QString maskStr = QString::number(mask);
              digi[digiID]->WriteValue(PHA::CH::ChannelsTriggerMask, maskStr.toStdString() , chID);
            }
          }; break;
          case 2 : { /// TRGIN, when the whole board is trigger by TRG-IN
            digi[digiID]->WriteValue(PHA::CH::EventTriggerSource, "TRGIN", chID);
            digi[digiID]->WriteValue(PHA::CH::WaveTriggerSource, "TRGIN", chID);
            digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "TRGIN", chID);
            
            digi[digiID]->WriteValue(PHA::CH::CoincidenceLength, "100", chID);
          }; break;
          case 3 : { /// disbaled
            digi[digiID]->WriteValue(PHA::CH::EventTriggerSource, "Disabled", chID);
            digi[digiID]->WriteValue(PHA::CH::WaveTriggerSource, "Disabled", chID);
            digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Disabled", chID);
          }; break;
        }
      }

    });

    UpdatePanelFromMemory();
    UpdateOtherPanels();

  }
  layout->addWidget(groupbox, row, col);
}

//^##############################################################
void SOLARISpanel::RefreshSettings(){
  for( int i = 0 ; i < nDigi; i++){
    if( digi[i]->IsDummy() || !digi[i]->IsConnected()){
      digi[i]->ReadAllSettings();
    }
  }
  UpdatePanelFromMemory();
}

void SOLARISpanel::UpdatePanelFromMemory(){

  if( !isVisible() ) return;

  enableSignalSlot = false;

  printf("SOLARISpanel::%s\n", __func__);

  //@===================== LineEdit and SpinBox
  for( int SettingID = 0; SettingID < (int) SettingItems.size() ; SettingID ++){
    for( int DigiID = 0; DigiID < (int) mapping.size(); DigiID ++){
      if( DigiID >= nDigi ) continue;;

      for( int chID = 0; chID < (int) mapping[DigiID].size(); chID++){

        if( mapping[DigiID][chID] < 0 ) continue;

        std::string haha = digi[DigiID]->GetSettingValue(SettingItems[SettingID], chID);
        sbSetting[SettingID][DigiID][chID]->setValue( atof(haha.c_str()));

        if( SettingItems[SettingID].GetPara() == PHA::CH::TriggerThreshold.GetPara() ){
          std::string haha =  digi[DigiID]->GetSettingValue(PHA::CH::SelfTrgRate, chID);
          leDisplay[SettingID][DigiID][chID]->setText(QString::fromStdString(haha));
        }else{
          leDisplay[SettingID][DigiID][chID]->setText(QString::fromStdString(haha));
        }

        haha = digi[DigiID]->GetSettingValue(PHA::CH::ChannelEnable, chID);
        chkOnOff[SettingID][DigiID][chID]->setChecked( haha == "True" ? true : false);
        leDisplay[SettingID][DigiID][chID]->setEnabled(haha == "True" ? true : false);
        sbSetting[SettingID][DigiID][chID]->setEnabled(haha == "True" ? true : false);
        
        ///printf("====== %d %d %d |%s|\n", SettingID, DigiID, chID, haha.c_str());
      }
    }
  }

  //@===================== Trigger
  for( int k = 0; k < detIDList.size() ; k++){
    if( detIDList[k][0] >= detMaxID[0] || 0 > detIDList[k][0]) continue;  //! only for array
    
    //if( detIDList[k].size() <= 2) continue;
    std::vector<unsigned long> triggerMap;
    std::vector<std::string> coincidentMask;
    std::vector<std::string> antiCoincidentMask;
    std::vector<std::string> eventTriggerSource;
    std::vector<std::string> waveTriggerSource;

    for( int h = 1; h < detIDList[k].size(); h++){
      int digiID = detIDList[k][h] >> 8;
      int chID = (detIDList[k][h] & 0xFF);

      triggerMap.push_back(Utility::TenBase(digi[digiID]->GetSettingValue(PHA::CH::ChannelsTriggerMask, chID)));
      coincidentMask.push_back(digi[digiID]->GetSettingValue(PHA::CH::CoincidenceMask, chID));
      antiCoincidentMask.push_back(digi[digiID]->GetSettingValue(PHA::CH::AntiCoincidenceMask, chID));
      eventTriggerSource.push_back(digi[digiID]->GetSettingValue(PHA::CH::EventTriggerSource, chID));
      waveTriggerSource.push_back(digi[digiID]->GetSettingValue(PHA::CH::WaveTriggerSource, chID));
    }

    //====== only acceptable condition is eventTriggerSource are all ChSelfTrigger
    //       and coincidentMask for e, xf, xn, are at least one for Ch64Trigger
    //       and waveTriggerSource are all ChSelfTrigger

    int detTypeID = FindDetTypID(detIDList[k]);
    //Check the 0-index
    bool isAcceptableSetting = true;
    bool isTriggerE = false;

    if( eventTriggerSource[0] != waveTriggerSource[0] ||  coincidentMask[0] != antiCoincidentMask[0]  ) isAcceptableSetting = false;
    if( antiCoincidentMask[0] != "Disabled") isAcceptableSetting = false;

    if( !(coincidentMask[0] == "Disabled" || coincidentMask[0] == "TRGIN") ) isAcceptableSetting = false;

    //check 0-index settings
    if( isAcceptableSetting ){
      if( eventTriggerSource[0] == "ChSelfTrigger" && coincidentMask[0] == "Disabled") {
        
        cbTrigger[detTypeID][detIDList[k][0]]->setCurrentText("Self Trigger");

        unsigned long mask = 1ULL << (detIDList[k][1] & 0xFF);

        for( int p = 1; p < (int) triggerMap.size(); p ++){
          if( coincidentMask[p] != "Ch64Trigger") isAcceptableSetting = false;
          if( triggerMap[p] != mask) isAcceptableSetting = false;
        }  
        if( isAcceptableSetting ) {
          cbTrigger[detTypeID][detIDList[k][0]]->setCurrentText("Trigger e");
          isTriggerE = true;
        }


      }else if( eventTriggerSource[0] == "Disabled" && coincidentMask[0] == "Disabled" ) {
        cbTrigger[detTypeID][detIDList[k][0]]->setCurrentText("Disabled");
      }else if( eventTriggerSource[0] == "TRGIN" && coincidentMask[0] == "TRGIN") {
        cbTrigger[detTypeID][detIDList[k][0]]->setCurrentText("Ext. Trigger");
      }else{
        isAcceptableSetting = false;
      }
    }

    if( isAcceptableSetting){
      //Check if eventTriggerSource or coincidentMask compare to the 0-index
      for( int h = 2; h < detIDList[k].size(); h++){
        if( eventTriggerSource[h-1] != eventTriggerSource[0]){
          isAcceptableSetting = false;
          break;
        }
        if( waveTriggerSource[h-1] != waveTriggerSource[0]){
          isAcceptableSetting = false;
          break;
        }
        if( !isTriggerE && coincidentMask[h-1] != coincidentMask[0]){
          isAcceptableSetting = false;
          break;
        }
        if( antiCoincidentMask[h-1] != antiCoincidentMask[0]){
          isAcceptableSetting = false;
          break;
        }
      }
    }
    if( !isAcceptableSetting ) cbTrigger[detTypeID][detIDList[k][0]]->setCurrentText("Others");
  }

  //@===================== Coin. time
  std::vector<int> coinTime;
  
  for( int i = 0; i < detIDList.size(); i++){
    for( int j = 1; j < detIDList[i].size(); j++){
      int digiID = detIDList[i][j] >> 8;
      int chID = (detIDList[i][j] & 0xFF);
      if( digiID >= nDigi ) continue;
      if( digi[digiID]->IsDummy() || !digi[digiID]->IsConnected() ) continue;
      coinTime.push_back( atoi(digi[digiID]->GetSettingValue(PHA::CH::CoincidenceLength, chID).c_str()));
    }
  }

  bool isSameCoinTime = true;
  for( int i = 1; i < (int) coinTime.size(); i++){
    if( coinTime[i] != coinTime[0]) {
      isSameCoinTime = false;
      break;
    }
  }

  if( isSameCoinTime ){
    sbCoinTime->setValue(coinTime[0]);
  }else{
    sbCoinTime->setValue(-1);
  }

  enableSignalSlot = true;

}

void SOLARISpanel::UpdateThreshold(){

  for( int SettingID = 0; SettingID < (int) SettingItems.size() ; SettingID ++){
    if( SettingItems[SettingID].GetPara() != PHA::CH::TriggerThreshold.GetPara() ) continue;

    for( int DigiID = 0; DigiID < (int) mapping.size(); DigiID ++){
      if( DigiID >= nDigi ) continue;;

      for( int chID = 0; chID < (int) mapping[DigiID].size(); chID++){

        if( mapping[DigiID][chID] < 0 ) continue;
        
        std::string haha =  digi[DigiID]->GetSettingValue(PHA::CH::SelfTrgRate, chID);
        leDisplay[SettingID][DigiID][chID]->setText(QString::fromStdString(haha));
        
        ///printf("====== %d %d %d |%s|\n", SettingID, DigiID, chID, haha.c_str());
      }
    }
  }
}

//^#########################################
void SOLARISpanel::SaveSettings(){


}

void SOLARISpanel::LoadSettings(){


}