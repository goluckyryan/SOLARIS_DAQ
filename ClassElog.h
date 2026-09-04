#ifndef CLASSELOG_H
#define CLASSELOG_H

#include <QObject>
#include <QString>
#include <QStringList>

const QString defaultElogPort = "443";
const int elogTimeout = 30000; // msec, max wait for the elog command to finish

//^#===================================================== Elog
/// Wrapper around the "elog" command line client.
/// It does no GUI, the progress/error text is emitted by logMsg().
class Elog : public QObject{
  Q_OBJECT

public:

  explicit Elog(QObject * parent = nullptr);
  ~Elog();

  void SetServer(QString ip, QString port, bool useSSL);
  void SetAuth(QString user, QString pwd);
  void SetLogbook(QString logbook); /// the logbook is the experiment name

  bool IsConfigured() const {return !elogIP.isEmpty() && !logbook.isEmpty();}

  /// write a test entry, then append to it. return true only when both work.
  bool Check();

  /// create a new entry. the ID of the new entry is kept for Append().
  bool Write(QString htmlText, QString subject = "", QString category = "", int runNumber = 0);

  /// append to the entry made by the last Write(). attachmentPath can be empty.
  bool Append(QString appendHtmlText, QString attachmentPath = "");

  /// 0 = ready, -1 = disabled or failed, >0 = ID of the current entry
  int  GetID() const {return elogID;}
  void SetID(int id) {elogID = id;}
  bool IsEnabled() const {return elogID >= 0;}

  QString GetLogbook() const {return logbook;}

signals:

  void logMsg(QString msg);

private:

  QStringList BaseArgs() const;   /// -h -p [-s] -l -u, shared by every call
  QString Execute(const QStringList & arg);
  static int ExtractID(const QString & output);

  QString elogIP;
  QString port;
  bool useSSL;
  QString logbook;
  QString user;
  QString pwd;

  int elogID;

};

#endif // CLASSELOG_H
