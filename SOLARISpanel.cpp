#include "SOLARISpanel.h"

#include <map>
#include <utility>

#include <QFile>
#include <QLabel>
#include <QSet>
#include <QList>
#include <QFrame>
#include <QFileDialog>

#define NCOL 10 // number of column

#define ChStartIndex 2 // the index of detIDList for channel

std::vector<Reg> SettingItems = {PHA::CH::TriggerThreshold, PHA::CH::DC_Offset};

//^====================================================== Trigger modes
/// One row per entry of the Trigger combo box, holding the four register settings that entry
/// writes. The panel used to hash those four strings into a hex nibble word (0x2211, 0x3331, ...)
/// and compare against magic constants; what each mode actually meant was only recoverable by
/// decoding the arithmetic, which is also where several read-back bugs hid.
///
/// Keep this in step with the write switch in CreateDetGroup -- the rows are transcribed from it.
/// AntiCoincidenceMask is "Disabled" in every mode; it is listed rather than assumed so a board
/// left with a stale anti-coincidence setting reads back as "Others" instead of being ignored.
namespace TriggerMode {

  struct Row {
    const char * name;            // what the combo box shows, for reference
    const char * evtTrgSource;    // PHA::CH::EventTriggerSource
    const char * waveTrgSource;   // PHA::CH::WaveTriggerSource
    const char * coinMask;        // PHA::CH::CoincidenceMask
    const char * antiCoinMask;    // PHA::CH::AntiCoincidenceMask
  };

  /// Index is the combo box index. "Trigger e" (1) is not here: it is the one position-dependent
  /// mode -- the group's first channel self-triggers and the rest sit in coincidence on it -- so
  /// it cannot be a single row. See IsTriggerE below.
  static const Row Table[] = {
    /* 0 */ { "Self Trigger", "ChSelfTrigger", "ChSelfTrigger", "Disabled", "Disabled" },
    /* 1 */ { nullptr       , nullptr        , nullptr        , nullptr   , nullptr    },
    /* 2 */ { "Ext. Trigger", "TRGIN"        , "TRGIN"        , "TRGIN"   , "Disabled" },
    /* 3 */ { "Disabled"    , "Disabled"     , "Disabled"     , "Disabled", "Disabled" },
  };

  const int Count  = 5;   // combo box entries, including "Others"
  const int Others = 4;

  /// Row index for these four settings, or -1 for none. Note that GetSettingValueFromMemory()
  /// hands back the literal "invalid" when a read fails, which matches no row -- callers that
  /// care about the difference must test for it themselves.
  inline int ModeOf(const std::string & evt, const std::string & wave,
                    const std::string & coin, const std::string & anti){
    for( int i = 0; i < (int) (sizeof(Table)/sizeof(Table[0])); i++ ){
      if( Table[i].name == nullptr ) continue;
      if( evt  == Table[i].evtTrgSource  && wave == Table[i].waveTrgSource &&
          coin == Table[i].coinMask      && anti == Table[i].antiCoinMask ) return i;
    }
    return -1;
  }

  /// "Trigger e" for the channel at position `pos` within its detector group.
  inline bool IsTriggerE(int pos, const std::string & evt, const std::string & wave,
                         const std::string & coin, const std::string & anti){
    if( evt != "ChSelfTrigger" || wave != "ChSelfTrigger" || anti != "Disabled" ) return false;
    return coin == ( pos == 0 ? "Disabled" : "Ch64Trigger" );
  }

}

SOLARISpanel::SOLARISpanel(Digitizer2Gen **digi, unsigned short nDigi, 
                          QString analysisPath,
                          std::vector<std::vector<int>> mapping, 
                          QStringList detType, 
                          QStringList detGroupName,
                          std::vector<int> detGroupID,
                          std::vector<int> detMaxID, 
                          QWidget *parent) : QWidget(parent){

  setWindowTitle("SOLARIS Settings");
  setGeometry(0, 0, 1350, 800);

  this->digi = digi;
  this->nDigi = nDigi;
  if( this->nDigi > MaxNumberOfDigitizer ) {
    this->nDigi = MaxNumberOfDigitizer;
    qDebug() << "Please increase the MaxNumberOfChannel";
  }
  this->mapping = mapping;
  this->detTypeNameList = detType;
  this->detMaxID = detMaxID;
  this->detGroupID = detGroupID;
  this->detGroupNameList = detGroupName;
  this->digiSettingPath = analysisPath + "/working/Settings/";

  //Check number of detector type. The ranges come from detMaxID in Mapping.h, not from here --
  //the production file is e 0-99, xf 100-199, xn 200-299, rdt 300-399, eNum 400-499.
  int nDetType = detTypeNameList.size();
  for( int k = 0 ; k < nDetType; k++) nDetinType.push_back(0);

  std::vector<int> condenGroupID = detGroupID;
  std::sort(condenGroupID.begin(), condenGroupID.end()); // sort the detGroupID
  auto last = std::unique(condenGroupID.begin(), condenGroupID.end());
  condenGroupID.erase(last, condenGroupID.end());
  int nDetGroup = condenGroupID.size();
  /// Counted by raw group ID, not by position in the condensed list, so it has to be wide enough
  /// for the largest ID rather than for the number of distinct ones.
  nDetinGroup.assign(MaxDetGroup, 0);


  QList<QList<int>> detIDListTemp; // store { detGroup, detID,  (Digi << 8 ) + ch}, making mapping into 1-D array to be consolidated
  printf("################################# \n");
  for( int i = 0; i < (int) mapping.size() ; i++){
    for( int j = 0; j < (int) mapping[i].size(); j++ ){
      printf("%4d,", mapping[i][j]);
      if( mapping[i][j] >= 0 ) {
        int groupID = FindDetGroup(mapping[i][j]);
        int typeID = FindDetTypeID(mapping[i][j]);
        /// A detID above detMaxID.back() has no type, so FindDetTypeID gives -1 and FindDetGroup
        /// gives -999; both were used as subscripts on the counters below.
        if( typeID < 0 || groupID < 0 || groupID >= MaxDetGroup ){
          printf("\n[SOLARISpanel] det-%d (digi %d, ch %d) matches no detector type -- ignored.\n", mapping[i][j], i, j);
        }else{
          QList<int> haha ;
          haha <<  groupID <<  mapping[i][j] << ((i << 8 ) + j);
          detIDListTemp <<  haha;
          nDetinType[typeID] ++;
          nDetinGroup[groupID] ++;
        }
      }
      if( j % 16 == 15 ) printf("\n");
    }
    printf("------------------ \n");
  }

  //consolidate detIDListTemp --> 2D array of (detID, (Digi << 8) +  ch, +.... )
  //for example, {2, 0, 0}, {2, 100, 1}, {2, 200, 2}--> {2, 0, 0, 1, 2};
  //for example, {2, 1, 3}, {2, 101, 4}, {2, 201, 5}--> {2, 0, 3, 4, 5};
  /// What makes two entries the same detector is the pair (group, offset within the detector's
  /// own type) -- that is what puts e-3, xf-103 and xn-203 on one row. So key a map by the pair
  /// and the consolidation is one pass.
  ///
  /// This used to be a nested scan over everything accumulated so far, recomputing
  /// FindDetTypeID() and the type's low bound four times per comparison -- O(n^2) lookups to
  /// answer a question the key already settles. The offset is now computed once per entry.
  detIDArrayList.clear();
  std::map<std::pair<int,int>, int> rowOf; // (group, offset within type) -> index in detIDArrayList
  for( int i = 0; i < detIDListTemp.size(); i++ ){
    const int groupID = detIDListTemp[i][0];
    const int detID   = detIDListTemp[i][1];
    const int typeID  = FindDetTypeID(detID);
    const int low     = (typeID == 0 ? 0 : detMaxID[typeID-1]);

    auto key = std::make_pair(groupID, detID - low);
    auto it  = rowOf.find(key);
    if( it == rowOf.end() ){
      rowOf[key] = detIDArrayList.size();
      detIDArrayList << detIDListTemp[i];
    }else{
      detIDArrayList[it->second] << detIDListTemp[i][2];
    }
  }

  //---- sort detIDList;
  std::sort(detIDArrayList.begin(), detIDArrayList.end(), [](const QList<int>& a, const QList<int>& b) {
    return a.at(1) < b.at(1);
  });

  //---- assign each detector a panel slot within its group.
  // The raw detID cannot be the subscript. Mapping.h numbers detectors by type, not by group --
  // with the production file the recoils are 300-307 -- while groupBox[][][] and cbTrigger[][] are
  // only MaxDetID wide, so groupBox[1][s][300] lands on another group's entry.
  // The right subscript is the offset within the detector's own type, which the consolidation loop
  // above already computes: it is what makes e-3, xf-103 and xn-203 one detector. That offset is
  // the detector's position in its group, it is stable when the map gains or loses a detector,
  // and for the array it is the 0-59 the panel was sized for.
  detSlotMap.clear();
  for( int i = 0; i < detIDArrayList.size(); i++ ){
    int groupID = detIDArrayList[i][0];
    int detID   = detIDArrayList[i][1];
    if( groupID < 0 || groupID >= MaxDetGroup ){
      printf("[SOLARISpanel] det-%d has group %d outside [0, %d) -- no panel entry.\n", detID, groupID, MaxDetGroup);
      continue;
    }
    int typeID = FindDetTypeID(detID);
    int low    = (typeID == 0 ? 0 : detMaxID[typeID-1]);
    int slot   = detID - low;
    if( slot < 0 || slot >= MaxDetID ){
      printf("[SOLARISpanel] det-%d is no. %d in its type; only %d fit on a tab, so it is not shown. Increase MaxDetID.\n", detID, slot, MaxDetID);
      continue;
    }
    detSlotMap.insert(detID, slot);
  }

  //------------- display detector summary
  //qDebug() << detIDList;
  printf("---------- num. of det. Type : %d\n", nDetType);
  for( int i =0; i < nDetType; i ++ ) {
    detType[i].remove(' ');
    printf(" Type-%d (%6s) : %3d det.  (%3d - %3d)\n", i, detType[i].toStdString().c_str(), nDetinType[i], (i==0 ? 0 : detMaxID[i-1]), detMaxID[i]-1);
  }
  printf("---------- num. of det. Group : %d\n", nDetGroup);
  for( int i =0; i < nDetGroup; i++){
    /// i is the position in the condensed list; nDetinGroup is counted by raw group ID.
    printf(" Group-%d (%10s) : %3d det.\n", condenGroupID[i], detGroupName[i].toStdString().c_str(), nDetinGroup[condenGroupID[i]]);
  }

  //---------- Set Panel
  QGridLayout * mainLayout = new QGridLayout(this); this->setLayout(mainLayout);

  ///=================================
  int rowIndex = 0 ;
  QPushButton * bnRefresh = new QPushButton("Refresh Settings", this);
  connect(bnRefresh, &QPushButton::clicked, this, &SOLARISpanel::RefreshSettings );
  mainLayout->addWidget(bnRefresh, rowIndex, 0, 1, 2);

  QPushButton * bnSaveSetting = new QPushButton("Save Settings", this);
  connect(bnSaveSetting, &QPushButton::clicked, this, &SOLARISpanel::SaveSettings);
  mainLayout->addWidget(bnSaveSetting, rowIndex, 2, 1, 2);  
  
  QPushButton * bnLoadSetting = new QPushButton("Load Settings", this);
  connect(bnLoadSetting, &QPushButton::clicked, this, &SOLARISpanel::LoadSettings);
  mainLayout->addWidget(bnLoadSetting, rowIndex, 4, 1, 2);

  QLabel * lbCoinTime = new QLabel("Coin. Time (all ch.) [ns]", this);
  lbCoinTime->setAlignment(Qt::AlignRight | Qt::AlignCenter);
  mainLayout->addWidget(lbCoinTime, rowIndex, 6, 1, 2);

  sbCoinTime = new RSpinBox(this);
  sbCoinTime->setMinimum(-1);
  sbCoinTime->setMaximum(atof(PHA::CH::CoincidenceLength.GetAnswers()[1].first.c_str()));
  sbCoinTime->setSingleStep(atof(PHA::CH::CoincidenceLength.GetAnswers()[2].first.c_str()));
  sbCoinTime->setDecimals(0);
  sbCoinTime->SetToolTip(atof(PHA::CH::CoincidenceLength.GetAnswers()[1].first.c_str()));
  mainLayout->addWidget(sbCoinTime, rowIndex, 8, 1, 2);

  
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
      if( i >= nDigi || digi[i]->IsDummy() || !digi[i]->IsConnected() ) continue;
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
  rowIndex ++;
  QLabel * info2 = new QLabel("The panel is defined by " + analysisPath + "/working/Mapping.h", this);
  mainLayout->addWidget(info2, rowIndex, 0, 1, 4);

  ///=================================
  rowIndex ++;
  QTabWidget * tabWidget = new QTabWidget(this); mainLayout->addWidget(tabWidget, rowIndex, 0, 1, 10);
  for( int gIndex = 0; gIndex < nDetGroup; gIndex ++ ){

    /// detGroupName is one entry per distinct group, so it is indexed by position in the condensed
    /// list, while detIDArrayList[][0] and the widget arrays are indexed by the raw group ID from
    /// the //C& line. The two only coincide when the IDs run 0,1,2,... -- keep them apart.
    const int detGroup = condenGroupID[gIndex];
    if( detGroup < 0 || detGroup >= MaxDetGroup ) continue;

    QTabWidget * tabSetting = new QTabWidget(tabWidget);
    tabWidget->addTab(tabSetting, detGroupName[gIndex]);

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

      chkAll[detGroup][SettingID] = new QCheckBox("Set for all", tab);
      layout->addWidget(chkAll[detGroup][SettingID], 0, 0);

      connect(chkAll[detGroup][SettingID], &QCheckBox::stateChanged, this, [=](bool state){
        bool found = false;
        for(int i = 0; i < detIDArrayList.size(); i++){
          if( detIDArrayList[i][0] != detGroup ) continue;
          if( found == false ){
            found = true;
            continue;
          }else{
            int slot = DetSlot(detIDArrayList[i][1]);
            if( slot < 0 || groupBox[detGroup][SettingID][slot] == nullptr ) continue;
            groupBox[detGroup][SettingID][slot]->setEnabled(!state);
          }
        }
      });
      
      QFrame *line = new QFrame(tab);
      line->setFrameShape(QFrame::HLine);
      line->setFrameShadow(QFrame::Sunken);
      line->setFixedHeight(10);
      layout->addWidget(line, 1, 0, 1, NCOL);

      for(int i = 0; i < detIDArrayList.size(); i++){
        if( detIDArrayList[i][0] != detGroup ) continue;
        /// Position by the detector's slot in its own group, not by i. i counts every detector in
        /// the panel, so the recoil tab used to start at row 8 with six blank rows above it.
        int slot = DetSlot(detIDArrayList[i][1]);
        if( slot < 0 ) continue;
        CreateDetGroup(SettingID, detIDArrayList[i], layout, slot/NCOL +  2, slot%NCOL);
      }

    }
  }

  enableSignalSlot = true;

}

SOLARISpanel::~SOLARISpanel(){

}

//^######################################################################
int SOLARISpanel::FindDetTypeID(int detID){
  for( int i = 0; i < (int) detTypeNameList.size(); i++){
    int lowID = (i == 0) ? 0 : detMaxID[i-1];
    if( lowID <= detID && detID < detMaxID[i]) {
      return i;
    }
  }
  return -1;
}

int SOLARISpanel::FindDetGroup(int detID){
  int typeID = FindDetTypeID(detID);
  if( typeID == -1) return -999;
  return detGroupID[typeID];
}

void SOLARISpanel::CreateDetGroup(int SettingID, QList<int> detIDArray, QGridLayout * &layout, int row, int col){

  int detGroup = detIDArray[0];
  int detID = detIDArray[1];

  /// The widget arrays are indexed by panel slot, not by detID -- see detSlotMap. A detector the
  /// constructor could not place has no slot and gets no widgets; it was reported there.
  const int slot = DetSlot(detID);
  if( detGroup < 0 || detGroup >= MaxDetGroup || slot < 0 ) return;

  groupBox[detGroup][SettingID][slot] = new QGroupBox("Det-" + QString::number(detID), this);
  groupBox[detGroup][SettingID][slot]->setFixedWidth(130);
  groupBox[detGroup][SettingID][slot]->setAlignment(Qt::AlignCenter);
  QGridLayout * layout0 = new QGridLayout(groupBox[detGroup][SettingID][slot]);
  layout0->setSpacing(0);
  layout0->setAlignment(Qt::AlignLeft);

  //@======================================== SpinBox and Display
  bool isDisableDetector = false;

  int nChInGroupBox = 0;
  for( int ppp = ChStartIndex; ppp < detIDArray.size(); ppp ++){

    int chIndex = ppp - ChStartIndex;
    isDisableDetector = false;
    nChInGroupBox ++;

    int digiID = (detIDArray[ppp] >> 8 );
    int chID = (detIDArray[ppp] & 0xFF);

    int typeID = FindDetTypeID(mapping[digiID][chID]);

    QLabel * lb  = new QLabel(detTypeNameList[typeID].remove(' '), this);  
    layout0->addWidget(lb, 2*chIndex, 0, 2, 1);

    chkOnOff[SettingID][digiID][chID] = new QCheckBox(this);
    layout0->addWidget(chkOnOff[SettingID][digiID][chID], 2*chIndex, 1, 2, 1);

    leDisplay[SettingID][digiID][chID] = new QLineEdit(this);
    leDisplay[SettingID][digiID][chID]->setFixedWidth(70);
    leDisplay[SettingID][digiID][chID]->setReadOnly(true);
    leDisplay[SettingID][digiID][chID]->setStyleSheet("color : green;");
    layout0->addWidget(leDisplay[SettingID][digiID][chID], 2*chIndex, 2);

    sbSetting[SettingID][digiID][chID] = new RSpinBox(this);
    if( digiID < nDigi ) sbSetting[SettingID][digiID][chID]->setToolTip( "Digi-" + QString::number(digi[digiID]->GetSerialNumber()) + ", Ch-" + QString::number(chID));
    sbSetting[SettingID][digiID][chID]->setToolTipDuration(-1);
    sbSetting[SettingID][digiID][chID]->setFixedWidth(70);

    sbSetting[SettingID][digiID][chID]->setMinimum(atoi(SettingItems[SettingID].GetAnswers()[0].first.c_str()));
    sbSetting[SettingID][digiID][chID]->setMaximum(atoi(SettingItems[SettingID].GetAnswers()[1].first.c_str()));
    sbSetting[SettingID][digiID][chID]->setSingleStep(atoi(SettingItems[SettingID].GetAnswers()[2].first.c_str()));

    layout0->addWidget(sbSetting[SettingID][digiID][chID], 2*chIndex+1, 2);

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

      if( chkAll[detGroup][SettingID]->isChecked() ){
        for(int k = 0; k < detIDArrayList.size() ; k++){
          if( detGroup == detIDArrayList[k][0] ){
            for( int h = ChStartIndex; h < detIDArrayList[k].size() ; h++){
              if( h != chIndex + ChStartIndex) continue;
              int digiK = (detIDArrayList[k][h] >> 8);
              if( digiK >= nDigi ) continue;
              int index = (detIDArrayList[k][h] & 0xFF);

              QString msg;
              msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[digiK]->GetSerialNumber());
              msg += ",CH:" + QString::number(index) + "(" + detTypeNameList[typeID] + ")";
              msg += " = " + QString::number(spb->value());
              
              if( digi[digiK]->WriteValue(para, std::to_string(spb->value()), index)){
                SendLogMsg(msg + "|OK.");
                spb->setStyleSheet("");
              }else{
                SendLogMsg(msg + "|Fail.");
                spb->setStyleSheet("color:red;");
              }
            }
          }
        }
      }else{
        QString msg;
        msg = QString::fromStdString(para.GetPara()) + "|DIG:"+ QString::number(digi[digiID]->GetSerialNumber());
        msg += ",CH:" + QString::number(chID) + "(" + detTypeNameList[typeID] + ")";
        msg += " = " + QString::number(spb->value());
        if( digi[digiID]->WriteValue(para, std::to_string(spb->value()), chID)){
          SendLogMsg(msg + "|OK.");
          spb->setStyleSheet("");
        }else{
          SendLogMsg(msg + "|Fail.");
          spb->setStyleSheet("color:red;");
        }
      }
      UpdatePanelFromMemory();
      UpdateOtherPanels();
    });
    
    ///===================== for the OnOff CheckBox
    connect(chkOnOff[SettingID][digiID][chID], &QCheckBox::stateChanged, this, [=](int state){
      if( !enableSignalSlot ) return; 

      if( chkAll[detGroup][SettingID]->isChecked() ){

        for(int k = 0; k < detIDArrayList.size() ; k++){
          if( detGroup == detIDArrayList[k][0] ){

            for( int h = ChStartIndex; h < detIDArrayList[k].size() ; h++){
              if( h != chIndex + ChStartIndex) continue;
              int digiK = (detIDArrayList[k][h] >> 8);
              if( digiK >= nDigi ) continue;
              int index = (detIDArrayList[k][h] & 0xFF);
              QString msg;
              msg = QString::fromStdString(PHA::CH::ChannelEnable.GetPara()) + "|DIG:"+ QString::number(digi[digiK]->GetSerialNumber());
              msg += ",CH:" + QString::number(index) + "(" + detTypeNameList[typeID] + ")";
              msg += ( state ? " = True" : " = False");
              
              if( digi[digiK]->WriteValue(PHA::CH::ChannelEnable, state ? "True" : "False", index)){
                SendLogMsg(msg + "|OK.");
                enableSignalSlot = false;

                leDisplay[SettingID][digiK][index]->setEnabled(state);
                sbSetting[SettingID][digiK][index]->setEnabled(state);
                chkOnOff [SettingID][digiK][index]->setChecked(state);
                enableSignalSlot = true;
              }else{
                SendLogMsg(msg + "|Fail.");
              }
            }
          }
        }

      }else{
        QString msg;
        msg = QString::fromStdString(PHA::CH::ChannelEnable.GetPara()) + "|DIG:"+ QString::number(digi[digiID]->GetSerialNumber());

        msg += ",CH:" + QString::number(chID) + "(" + detTypeNameList[typeID] + ")";
        msg += ( state ? " = True" : " = False");

        qDebug() << msg;
        
        if( digi[digiID]->WriteValue(PHA::CH::ChannelEnable, state ? "True" : "False", chID)){
          SendLogMsg(msg + "|OK.");
          enableSignalSlot = false;
          leDisplay[SettingID][digiID][chID]->setEnabled(state);
          sbSetting[SettingID][digiID][chID]->setEnabled(state);
          chkOnOff [SettingID][digiID][chID]->setChecked(state);
          enableSignalSlot = true;
        }else{
          SendLogMsg(msg + "|Fail.");
        }
      }

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

  if( SettingItems[SettingID].GetPara() == PHA::CH::TriggerThreshold.GetPara()){
    cbTrigger[detGroup][slot] = new RComboBox(this);
    cbTrigger[detGroup][slot]->addItem("Self Trigger", "ChSelfTrigger");    /// no coincident
    cbTrigger[detGroup][slot]->addItem("Trigger e", 0x1); // Self-trigger and coincient Ch64Trigger
    cbTrigger[detGroup][slot]->addItem("Ext. Trigger", "TRGIN");  // with coincident with TRGIN.
    cbTrigger[detGroup][slot]->addItem("Disabled", "Disabled");  // no Trigger, no coincident, basically channel still alive, but no recording
    cbTrigger[detGroup][slot]->addItem("Others", -999); // other settings

    layout0->addWidget(cbTrigger[detGroup][slot], 2*nChInGroupBox, 0, 1, 3);

    if( isDisableDetector ) cbTrigger[detGroup][slot]->setEnabled(false);

    //*========== connection
    connect(cbTrigger[detGroup][slot], &RComboBox::currentIndexChanged, this , [=](int index){
      if( !enableSignalSlot) return;

      if( cbTrigger[detGroup][slot]->itemData(index).toInt() == -999 ) return;

      if( chkAll[detGroup][SettingID]->isChecked() ){

        for( int k = 0; k < detIDArrayList.size(); k++){
          if( detIDArrayList[k][0] != detGroup ) continue;
          int hahaSlot = DetSlot(detIDArrayList[k][1]);
          if( hahaSlot < 0 || hahaSlot == slot ) continue;
          if( cbTrigger[detGroup][hahaSlot] == nullptr ) continue;
          cbTrigger[detGroup][hahaSlot]->setCurrentIndex(index);
        }

      }

      ///----------- single
      for( int i = ChStartIndex ; i < detIDArray.size(); i++){

        int digiID = (detIDArray[i] >> 8 );
        if( digiID >= nDigi) continue;
        int chID = (detIDArray[i] & 0xFF);

        if( digi[digiID]->IsDummy() || !digi[digiID]->IsConnected() ) continue;

        digi[digiID]->WriteValue(PHA::CH::AntiCoincidenceMask, "Disabled", chID);

        switch(index){
          case 0 : { /// Self Trigger
            digi[digiID]->WriteValue(PHA::CH::EventTriggerSource, "ChSelfTrigger", chID);
            digi[digiID]->WriteValue(PHA::CH::WaveTriggerSource, "ChSelfTrigger", chID);
            digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Disabled", chID);
          }; break;
          case 1 : { /// trigger by energy
            digi[digiID]->WriteValue(PHA::CH::EventTriggerSource, "ChSelfTrigger", chID);
            digi[digiID]->WriteValue(PHA::CH::WaveTriggerSource, "ChSelfTrigger", chID);

            if( i == ChStartIndex ){
              digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Disabled", chID);
            }else {
              digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Ch64Trigger", chID);
              digi[digiID]->WriteValue(PHA::CH::CoincidenceLength, "100");

              //Form the trigger bit
              unsigned long mask = 1ULL << (detIDArray[ChStartIndex] & 0xFF ); // trigger by energy
              QString maskStr = QString::number(mask);
              digi[digiID]->WriteValue(PHA::CH::ChannelsTriggerMask, maskStr.toStdString() , chID);
            }
          }; break;
          case 2 : { /// TRGIN, when the whole board is trigger by TRG-IN
            digi[digiID]->WriteValue(PHA::CH::EventTriggerSource, "TRGIN", chID);
            digi[digiID]->WriteValue(PHA::CH::WaveTriggerSource, "TRGIN", chID);
            digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "TRGIN", chID);
            
            digi[digiID]->WriteValue(PHA::CH::CoincidenceLength, "100");
          }; break;
          case 3 : { /// disbaled
            digi[digiID]->WriteValue(PHA::CH::EventTriggerSource, "Disabled", chID);
            digi[digiID]->WriteValue(PHA::CH::WaveTriggerSource, "Disabled", chID);
            digi[digiID]->WriteValue(PHA::CH::CoincidenceMask, "Disabled", chID);
          }; break;
        }

        SendLogMsg("SOLARIS panel : Set Trigger = " + cbTrigger[detGroup][slot]->itemText(index) + "|Digi:" + QString::number(digi[digiID]->GetSerialNumber()) + ",Det:" + QString::number(detID));

      }
      UpdatePanelFromMemory();
      UpdateOtherPanels();

    });


  }
  layout->addWidget(groupBox[detGroup][SettingID][slot], row, col);
}

//^##############################################################
void SOLARISpanel::RefreshSettings(){
  for( int i = 0 ; i < nDigi; i++){
    if( digi[i]->IsDummy() || !digi[i]->IsConnected()) continue;
    digi[i]->ReadAllSettings();
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

        std::string haha = digi[DigiID]->GetSettingValueFromMemory(SettingItems[SettingID], chID);
        sbSetting[SettingID][DigiID][chID]->setValue( atof(haha.c_str()));

        if( SettingItems[SettingID].GetPara() == PHA::CH::TriggerThreshold.GetPara() ){
          std::string haha =  digi[DigiID]->GetSettingValueFromMemory(PHA::CH::SelfTrgRate, chID);
          leDisplay[SettingID][DigiID][chID]->setText(QString::number(atof(haha.c_str()), 'f', 2) );
        }else{
          leDisplay[SettingID][DigiID][chID]->setText(QString::number(atof(haha.c_str()), 'f', 2) );
        }

        haha = digi[DigiID]->GetSettingValueFromMemory(PHA::CH::ChannelEnable, chID);
        chkOnOff[SettingID][DigiID][chID]->setChecked( haha == "True" ? true : false);
        leDisplay[SettingID][DigiID][chID]->setEnabled(haha == "True" ? true : false);
        sbSetting[SettingID][DigiID][chID]->setEnabled(haha == "True" ? true : false);
        
        //printf("====== %d %d %d |%s|\n", SettingID, DigiID, chID, haha.c_str());
      }
    }
  }

  //@===================== Trigger
  for( int k = 0; k < detIDArrayList.size() ; k++){
    if( detIDArrayList[k][1] >= detMaxID[0] || 0 > detIDArrayList[k][1]) continue;  //! only for array
    
    if( detIDArrayList[k][0] != 0 ) continue;

    //if( detIDList[k].size() <= 2) continue;
    bool skipFlag = false;
    std::vector<unsigned long> triggerMap;
    std::vector<std::string> coincidentMask;
    std::vector<std::string> antiCoincidentMask;
    std::vector<std::string> eventTriggerSource;
    std::vector<std::string> waveTriggerSource;

    for( int h = ChStartIndex; h < detIDArrayList[k].size(); h++){
      int digiID = detIDArrayList[k][h] >> 8;
      if( digiID >= nDigi) {
        skipFlag = true;
        continue;
      }

      int chID = (detIDArrayList[k][h] & 0xFF);
      triggerMap.push_back(Utility::TenBase(digi[digiID]->GetSettingValueFromMemory(PHA::CH::ChannelsTriggerMask, chID)));
      coincidentMask.push_back(digi[digiID]->GetSettingValueFromMemory(PHA::CH::CoincidenceMask, chID));
      antiCoincidentMask.push_back(digi[digiID]->GetSettingValueFromMemory(PHA::CH::AntiCoincidenceMask, chID));
      eventTriggerSource.push_back(digi[digiID]->GetSettingValueFromMemory(PHA::CH::EventTriggerSource, chID));
      waveTriggerSource.push_back(digi[digiID]->GetSettingValueFromMemory(PHA::CH::WaveTriggerSource, chID));
    }

    if(skipFlag) continue;

    //====== only acceptable condition is eventTriggerSource are all ChSelfTrigger
    //       and coincidentMask for e, xf, xn, are at least one for Ch64Trigger
    //       and waveTriggerSource are all ChSelfTrigger

    const int nChInDet = (int) triggerMap.size();
    /// A group with no readable channel has nothing to say about its mode, and the old code fell
    /// through to "Others" only because every count was 0 and so was the size it compared against.
    if( nChInDet == 0 ) continue;

    /// Count, per combo box entry, how many of this group's channels are in that mode; the group
    /// is in a mode only when every one of its channels is.
    int nChInMode[TriggerMode::Count] = {0};
    bool anyUnreadable = false;
    for( int i = 0; i < nChInDet; i++ ){
      if( eventTriggerSource[i] == "invalid" || waveTriggerSource[i] == "invalid" ||
          coincidentMask[i]     == "invalid" || antiCoincidentMask[i] == "invalid" ) anyUnreadable = true;

      int mode = TriggerMode::ModeOf(eventTriggerSource[i], waveTriggerSource[i],
                                     coincidentMask[i], antiCoincidentMask[i]);
      if( mode >= 0 ) nChInMode[mode] ++;

      /// Only for a real group. A lone self-triggering channel satisfies the Trigger-e predicate
      /// at position 0 as well, and that ambiguity is what used to make every single-channel
      /// detector read back as "Trigger E".
      if( nChInDet > 1 && TriggerMode::IsTriggerE(i, eventTriggerSource[i], waveTriggerSource[i],
                                                  coincidentMask[i], antiCoincidentMask[i]) ) nChInMode[1] ++;
    }

    /// First match wins. The old loop had no break, so where two buckets both matched it silently
    /// took the higher index.
    int comboxIndex = TriggerMode::Others;
    for( int i = 0; i < TriggerMode::Count; i++){
      if( nChInMode[i] == nChInDet ){ comboxIndex = i; break; }
    }

    if( anyUnreadable ){
      /// "invalid" is what GetSettingValueFromMemory returns when a read fails, and it matches no
      /// mode -- so a dead board looked exactly like a deliberately odd trigger setup. Say so.
      printf("[SOLARISpanel] det-%d : could not read back one or more trigger settings.\n", detIDArrayList[k][1]);
    }

    // if Trigger e, the later channels must be in coincidence on the first channel's bit
    if( comboxIndex == 1){
      const int firstCh = detIDArrayList[k][ChStartIndex] & 0xFF;
      /// The shift count is a raw channel id off the mapping file. 64 or above is undefined
      /// behaviour, not a large number.
      if( firstCh >= 64 ){
        comboxIndex = TriggerMode::Others;
      }else{
        const unsigned long ShouldBeMask = 1ULL << firstCh;
        /// From i = 1: the first channel's CoincidenceMask is "Disabled", so its trigger mask is
        /// unused and the write path deliberately leaves it alone. Checking it against 0 would
        /// reject valid setups over a stale register.
        for( int i = 1; i < nChInDet; i ++){
          if( triggerMap[i] != ShouldBeMask) comboxIndex = TriggerMode::Others;
        }
      }
    }

    int detGroup = FindDetGroup(detIDArrayList[k][1]);
    int slot     = DetSlot(detIDArrayList[k][1]);
    if( detGroup < 0 || detGroup >= MaxDetGroup || slot < 0 ) continue;
    if( cbTrigger[detGroup][slot] == nullptr ) continue;
    cbTrigger[detGroup][slot]->setCurrentIndex(comboxIndex);

  }

  //@===================== Coin. time
  std::vector<int> coinTime;
  
  for( int i = 0; i < detIDArrayList.size(); i++){
    for( int j = ChStartIndex; j < detIDArrayList[i].size(); j++){
      int digiID = detIDArrayList[i][j] >> 8;
      int chID = (detIDArrayList[i][j] & 0xFF);
      if( digiID >= nDigi ) continue;
      if( digi[digiID]->IsDummy() || !digi[digiID]->IsConnected() ) continue;
      coinTime.push_back( atoi(digi[digiID]->GetSettingValueFromMemory(PHA::CH::CoincidenceLength, chID).c_str()));
    }
  }

  bool isSameCoinTime = true;
  for( int i = 1; i < (int) coinTime.size(); i++){
    if( coinTime[i] != coinTime[0]) {
      isSameCoinTime = false;
      break;
    }
  }

  /// coinTime only collects non-dummy, connected boards, so an all-dummy bench session leaves it
  /// empty while isSameCoinTime is still true. -1 is the spin box's own "no common value".
  if( !coinTime.empty() && isSameCoinTime ){
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
        
        std::string haha =  digi[DigiID]->GetSettingValueFromMemory(PHA::CH::SelfTrgRate, chID);
        leDisplay[SettingID][DigiID][chID]->setText(QString::fromStdString(haha));
        
        ///printf("====== %d %d %d |%s|\n", SettingID, DigiID, chID, haha.c_str());
      }
    }
  }
}

//^#########################################
void SOLARISpanel::SaveSettings(){

  for(int i = 0; i < nDigi; i++){

    //*------ search for settings_XXXX.dat

    //Check path exist
    QDir dir(digiSettingPath);
    if( !dir.exists() ) dir.mkpath(".");

    QString settingFile = digiSettingPath + "/setting_" + QString::number(digi[i]->GetSerialNumber()) + "_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".dat";


    int flag = digi[i]->SaveSettingsToFile(settingFile.toStdString().c_str());
  
    switch (flag) {
      case 1 : {
        SendLogMsg("Saved setting file <b>" +  settingFile + "</b>.");
      }; break;
      case 0 : {
        SendLogMsg("<font style=\"color:red;\"> Fail to write setting file. <b> " + settingFile + " </b></font>");
      }; break;
      case -1 : {
        SendLogMsg("<font style=\"color:red;\"> Fail to save setting file <b> " + settingFile + " </b>, same settings are empty.</font>");
      }; break;
    };

  }
}

void SOLARISpanel::LoadSettings(){
  for(int i = 0; i < nDigi; i++){

    //*------ search for settings_XXXX.dat
    QString settingFile = digiSettingPath + "/settings_" + QString::number(digi[i]->GetSerialNumber()) + ".dat";
    if( digi[i]->LoadSettingsFromFile( settingFile.toStdString().c_str() ) ){
      SendLogMsg("Found setting file <b>" + settingFile + "</b> and loading. please wait.");
      digi[i]->SetSettingFileName(settingFile.toStdString());
      SendLogMsg("done settings.");
    }else{
      SendLogMsg("<font style=\"color: red;\">Unable to found setting file <b>" + settingFile + "</b>. </font>");
      digi[i]->SetSettingFileName("");
    }
    
  }

}