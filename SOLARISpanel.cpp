#include "SOLARISpanel.h"

#include <QFile>
#include <QSet>
#include <QList>

SOLARISpanel::SOLARISpanel(Digitizer2Gen **digi, unsigned short nDigi, 
                          std::vector<std::vector<int>> mapping, 
                          QStringList detType, 
                          std::vector<int> detMaxID, 
                          QWidget *parent) : QWidget(parent){

  setWindowTitle("SOLARIS Settings");
  setGeometry(0, 0, 1850, 1000);

  printf("%s\n", __func__);

  this->digi = digi;
  this->nDigi = nDigi;
  this->mapping = mapping;
  this->detType = detType;
  this->detMaxID = detMaxID;

  //Check number of detector type; Array 0-199, Recoil 200-299, other 300-
  int nDetType = detType.size();

  int nDet[nDetType];
  for( int k = 0 ; k < nDetType; k++) nDet[k] = 0;

  QList<int> detIDList; //consolidated

  for( int i = 0; i < (int) mapping.size() ; i++){
    for( int j = 0; j < (int) mapping[i].size(); j++ ){
      printf("%3d,", mapping[i][j]);
      if( mapping[i][j] >= 0 ) detIDList << mapping[i][j];
      for( int k = 0 ; k < nDetType; k++){
        int lowID = (k == 0 ? 0 : detMaxID[k-1]);     
        if( lowID <= mapping[i][j] && mapping[i][j] < detMaxID[k] ) nDet[k] ++ ;
      }
      if( j % 16 == 0 ) printf("\n");
    }
    printf("------------------ \n");
  }

  //----- consolidate detIDList;
  QSet<int> mySet(detIDList.begin(), detIDList.end());
  detIDList = mySet.values();

  for( int i = 0 ; i < detIDList.size(); i++) printf("%d\n", detIDList[i]);

  QVBoxLayout * mainLayout = new QVBoxLayout(this); this->setLayout(mainLayout);
  QTabWidget * tabWidget = new QTabWidget(this); mainLayout->addWidget(tabWidget);

  for( int detTypeID = 0; detTypeID < nDetType; detTypeID ++ ){

    QWidget * tab = new QWidget(this);
    tabWidget->addTab(tab, detType[detTypeID]);

    QGridLayout * layout = new QGridLayout(tab);
    layout->setAlignment(Qt::AlignLeft|Qt::AlignTop);
    layout->setSpacing(0);

    //QLineEdit * leNDet = new QLineEdit(QString::number(nDet[detTypeID]), tab);
    //layout->addWidget(leNDet, 0, 0);

    //ranege of detID 
    int lowID = (detTypeID == 0 ? 0 : detMaxID[detTypeID-1]);     

    for(int i = 0; i < detIDList.size(); i++){
      if( detIDList[i] < lowID || detIDList[i] > detMaxID[detTypeID]) continue;
      CreateSpinBoxGroup(PHA::CH::TriggerThreshold, i, layout, i/20, i%20);
    }

  }



}

SOLARISpanel::~SOLARISpanel(){

}

void SOLARISpanel::CreateSpinBoxGroup(const Reg para, int detID, QGridLayout * &layout, int row, int col){

  //find all chID = (iDigi << 8 + ch) for detID
  std::vector<int> chID;
  for( int i = 0; i < (int) mapping.size() ; i++){
    for( int j = 0; j < (int) mapping[i].size(); j++ ){
      if( mapping[i][j] == detID ) chID.push_back((i << 8)  + j);
    }
  }

  QGroupBox * groupbox = new QGroupBox("det-" + QString::number(detID), this);
  groupbox->setFixedWidth(80);
  QVBoxLayout * layout0 = new QVBoxLayout(groupbox);

  for( int i = 0; i < (int) chID.size(); i ++){
    QLineEdit * leTrigRate = new QLineEdit(this);
    leTrigRate->setFixedWidth(50);
    layout0->addWidget(leTrigRate);

    RSpinBox * sbThre = new RSpinBox(this);
    sbThre->setToolTip( "Digi-,Ch-" + QString::number(chID[i]));
    sbThre->setToolTipDuration(-1);
    sbThre->setFixedWidth(50);
    layout0->addWidget(sbThre);
  }

  layout->addWidget(groupbox, row, col);

}

void SOLARISpanel::CreateTab(const Reg para){


}