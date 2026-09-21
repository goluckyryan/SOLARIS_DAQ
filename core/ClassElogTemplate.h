#ifndef CLASSELOGTEMPLATE_H
#define CLASSELOGTEMPLATE_H

#include <QString>
#include <QStringList>

#include <functional>

const QString defaultElogTemplateFileName = "elog.template";

enum class ElogSection { StartRun, StopRun };

//^#===================================================== ElogTemplate
/// Renders the elog entry text from a user editable template file.
///
/// The file has two sections, opened by "#=== start Run" and "#=== stop Run".
/// Inside a section, <Name> is replaced by a variable. Anything that does not
/// look like a variable is passed through, so <br />, <b> and <font ...> keep
/// working (the elog client is called with -n 2, i.e. HTML).
///
/// A line holding a <Bd:Prop> token is emitted once per board, a line holding a
/// <Bd:Ch:Prop> token once per board and channel. <Bd3:Prop> picks one board and
/// does not repeat.
///
/// It knows nothing about digitizers, the owner supplies the values through
/// SetVar() and the boardVar/chVar callbacks.
class ElogTemplate {

public:

  ElogTemplate();

  /// read and parse the file. false when it cannot be read or holds no section.
  bool Load(const QString & filePath);

  bool Has(ElogSection sec) const;

  void ClearVars();
  void SetVar(const QString & name, const QString & value);
  void SetVar(const QString & name, int value);

  /// number of boards, and whether a board should be looped over (dummies are skipped)
  std::function<int()>        nBoard;
  std::function<bool(int bd)> boardValid;
  std::function<int(int bd)>  nChannel;

  /// resolve <Bd:prop> and <Bd:Ch:prop>. set ok = false for an unknown property.
  std::function<QString(int bd, const QString & prop, bool & ok)>          boardVar;
  std::function<QString(int bd, int ch, const QString & prop, bool & ok)>  chVar;

  /// the rendered HTML. unknown tokens are kept verbatim and listed in unresolved.
  QString Render(ElogSection sec, QStringList * unresolved = nullptr) const;

  /// "#Subject:" / "#Category:" of the section, already expanded. empty when not given.
  QString Subject(ElogSection sec, QStringList * unresolved = nullptr) const;
  QString Category(ElogSection sec, QStringList * unresolved = nullptr) const;

  QString GetFilePath() const {return filePath;}
  QString GetErrorMsg() const {return errorMsg;}

  /// the template shipped with the program, used to create a missing file
  static QString DefaultTemplate();

private:

  struct Section {
    QStringList body;
    QString subject;
    QString category;
    bool exist = false;
  };

  const Section & Get(ElogSection sec) const {return sec == ElogSection::StartRun ? startRun : stopRun;}

  /// what the line has to be repeated over. fixedBd is the board of a <BdN:Ch:...> token,
  /// it is only meaningful when loopCh is true and loopBd is false.
  static void ScanLoop(const QString & line, bool & loopBd, bool & loopCh, int & fixedBd);

  /// replace every known token of line. bd/ch are the current loop indexes, -1 when not looping.
  QString ExpandLine(const QString & line, int bd, int ch, QStringList * unresolved) const;

  /// resolve one token name. ok = false leaves the token untouched.
  QString Resolve(const QString & name, int bd, int ch, bool & ok) const;

  static QString Escape(const QString & text);

  Section startRun;
  Section stopRun;

  QStringList varName;
  QStringList varValue;

  QString filePath;
  QString errorMsg;

};

#endif // CLASSELOGTEMPLATE_H
