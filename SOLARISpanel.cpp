#include "SOLARISpanel.h"

#include <QFile>

SOLARISpanel::SOLARISpanel(Digitizer2Gen **digi, unsigned short nDigi, 
                          std::vector<std::vector<int>> mapping, 
                          QStringList detType, 
                          std::vector<int> detMaxID, 
                          QWidget *parent) : QWidget(parent){

  setWindowTitle("SOLARIS Settings");
  setGeometry(0, 0, 1850, 1000);

  this->digi = digi;
  this->nDigi = nDigi;
  this->mapping = mapping;
  this->detType = detType;
  this->detMaxID = detMaxID;

  //Check number of detector type; Array 0-199, Recoil 200-299, other 300-
  int nDetType = detType.size();

  int nDet[nDetType];
  for( int k = 0 ; k < nDetType; k++) nDet[k] = 0;

  for( int i = 0; i < (int) mapping.size() ; i++){
    for( int j = 0; j < (int) mapping[i].size(); j++ ){
      for( int k = 0 ; k < nDetType; k++){
        int lowID = (k == 0 ? 0 : detMaxID[k-1]);     
        if( lowID <= mapping[i][j] && mapping[i][j] < detMaxID[k] ) nDet[k] ++ ;
      }
    }
  }

  QVBoxLayout * mainLayout = new QVBoxLayout(this); this->setLayout(mainLayout);
  QTabWidget * tabWidget = new QTabWidget(this); mainLayout->addWidget(tabWidget);

  for( int detTypeID = 0; detTypeID < nDetType; detTypeID ++ ){

    QWidget * tab = new QWidget(this);
    tabWidget->addTab(tab, detType[detTypeID]);

    QGridLayout * layout = new QGridLayout(tab);

    //QLineEdit * leNDet = new QLineEdit(QString::number(nDet[detTypeID]), tab);
    //layout->addWidget(leNDet, 0, 0);

    ///Create threshold tab

  }



}

SOLARISpanel::~SOLARISpanel(){

}

void SOLARISpanel::CreateSpinBoxGroup(const Reg para, int detID, QGridLayout * &layout, int row, int col){

  //QLineEdit * leTrigRate



}

void SOLARISpanel::CreateTab(const Reg para){


}