#include "ClassElog.h"

#include <QProcess>

Elog::Elog(QObject * parent) : QObject(parent){
  elogIP  = "";
  port    = defaultElogPort;
  useSSL  = true;
  logbook = "";
  user    = "";
  pwd     = "";
  elogID  = 0;
}

Elog::~Elog(){}

void Elog::SetServer(QString ip, QString port, bool useSSL){
  this->elogIP = ip;
  this->port   = port.isEmpty() ? defaultElogPort : port;
  this->useSSL = useSSL;
}

void Elog::SetAuth(QString user, QString pwd){
  this->user = user;
  this->pwd  = pwd;
}

void Elog::SetLogbook(QString logbook){
  this->logbook = logbook;
}

QStringList Elog::BaseArgs() const{
  QStringList arg;
  arg << "-h" << elogIP << "-p" << port;
  /// without -s, an elogd behind https answers "400 The plain HTTP request was sent to HTTPS port"
  if( useSSL ) arg << "-s";
  arg << "-l" << logbook << "-u" << user << pwd;
  return arg;
}

QString Elog::Execute(const QStringList & arg){

  // printf("Elog command: %s\n", arg.join(" ").toStdString().c_str());

  QProcess elogBash;
  elogBash.start("elog", arg);

  if( !elogBash.waitForStarted(3000) ){
    emit logMsg("<font style=\"color : red;\">Cannot run the <b>elog</b> command. Is the elog client installed and in the PATH?</font>");
    return QString();
  }

  if( !elogBash.waitForFinished(elogTimeout) ){
    elogBash.kill();
    elogBash.waitForFinished(1000);
    emit logMsg("<font style=\"color : red;\">The <b>elog</b> command did not finish within " + QString::number(elogTimeout/1000) + " sec. Killed.</font>");
    return QString();
  }

  return QString::fromUtf8(elogBash.readAllStandardOutput());
}

int Elog::ExtractID(const QString & output){
  int index = output.indexOf("ID=");
  if( index == -1 ) return -1;
  return output.mid(index + 3).toInt();
}

bool Elog::Check(){

  if( elogIP.isEmpty() ){
    emit logMsg("No Elog IP. No elog will be used.");
    elogID = -1;
    return false;
  }

  if( logbook.isEmpty() ){
    emit logMsg("No Elog logbook (Exp Name). No elog will be used.");
    elogID = -1;
    return false;
  }

  if( !Write("Checking elog writing", "Testing communication", "checking") ){
    emit logMsg("<font style=\"color : red;\">Checked Elog Write. FAIL. (no elog will be used.) \
(check the logbook <b>" + logbook + "</b> exists, and the user, password, port and SSL setting) </font>");
    return false;
  }
  emit logMsg("Checked Elog writing. OK.");

  if( !Append("Check Elog append.") ){
    emit logMsg("<font style=\"color : red;\">Checked Elog Append. FAIL. (no elog will be used.) </font>");
    return false;
  }
  emit logMsg("Checked Elog Append. OK.");

  return true;
}

bool Elog::Write(QString htmlText, QString subject, QString category, int runNumber){

  if( !IsConfigured() ) return false;

  QStringList arg = BaseArgs();
  arg << "-a" << "Author=SOLARIS_DAQ";
  if( runNumber > 0 ) arg << "-a" << "RunNo=" + QString::number(runNumber);
  if( category != "" ) arg << "-a" << "Category=" + category;

  arg << "-a" << "Subject=" + subject
      << "-n" << "2" << htmlText;

  elogID = ExtractID(Execute(arg));

  return elogID > 0;
}

bool Elog::Append(QString appendHtmlText, QString attachmentPath){

  if( !IsConfigured() ) return false;
  if( elogID < 1 ) return false;

  /// retrieve the entry first, the elog client replaces the text, it does not append
  QStringList arg = BaseArgs();
  arg << "-w" << QString::number(elogID);

  QString output = Execute(arg);

  const QString separator = "========================================";

  int index = output.indexOf(separator);
  if( index == -1 ){
    elogID = -1;
    return false;
  }

  QString originalHtml = output.mid(index + separator.length());

  arg = BaseArgs();
  arg << "-e" << QString::number(elogID)
      << "-n" << "2" << originalHtml + "<br>" + appendHtmlText;

  if( !attachmentPath.isEmpty() ) arg << "-f" << attachmentPath;

  elogID = ExtractID(Execute(arg));

  return elogID > 0;
}
