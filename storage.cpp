
#include <QDir>
#include <QDebug>

#include "types.h"
#include "storage.h"

const QString storage_t::tmpsuffix = ".davqt";


struct storage_t::Pimpl {
  Pimpl(const QString& r, const QString& p) : root(r), path_dir(p) {}
  
  const QDir root;
  const QDir path_dir;
  QString path;
};


storage_t::storage_t(const QString& root, const QString& path)
  : p_(new Pimpl(root, root + QDir::separator() + path))
{
  if (!p_->root.exists()) {
    if (!p_->root.mkpath(".")) {
      throw qt_exception_t(QString("Can't use root dir [%1]").arg(p_->root.absolutePath()));
    }
  }
  
  if (!p_->path_dir.exists()) {
    if (!p_->path_dir.mkpath(".")) {
      throw qt_exception_t(QString("Can't use path dir [%1]").arg(p_->path_dir.absolutePath()));
    }
  }
  
  p_->path = p_->root.relativeFilePath(p_->path_dir.absolutePath());

  qDebug() << QString("Storage created at [%1]/[%2]").arg(p_->root.absolutePath()).arg(p_->path);
}

storage_t::~storage_t()
{
}

QString storage_t::root() const
{
  return p_->root.absolutePath();
}

QString storage_t::path() const
{
  return p_->path;
}

QString storage_t::prefix() const
{
  return p_->root.absolutePath();
}

QFileInfo storage_t::info(const QString& file) const
{
  return (file.startsWith(prefix()))
    ? QFileInfo(file)
    : QFileInfo(prefix() + QDir::separator() + file);
}

QString storage_t::file(const QString& file) const
{
  return info(file).fileName();
}

QString storage_t::folder(const QString& file) const
{
  QString dirname = info(file).dir().absolutePath() + "/";
  return dirname.replace(prefix(), "").replace(QRegExp("^[/]*"), "").replace("//", "/");
}

QString storage_t::file_path(const QString& file) const
{
  return QString(folder(file) + QDir::separator() + this->file(file)).replace(QRegExp("^[/]*"), "").replace("//", "/");
}

QString storage_t::absolute_file_path(const QString& file) const
{
  return QString(p_->root.absolutePath() + QDir::separator() + p_->path + QDir::separator() + file_path(file)).replace("//", "/");
}

QString storage_t::remote_file_path(const QString& file) const
{
  return QString("/" + p_->path + "/" + file_path(file)).replace("//", "/");
}

QString storage_t::local_info(const QString& key) const
{
  return QFileInfo(absolute_file_path(key));
}

std::unique_ptr<QFile> storage_t::local_file(const QString& key) const
{
  std::unique_ptr<QFile> file(new QFile(absolute_file_path(key)));
  if (!file->open(QIODevice::ReadWrite)) 
    throw qt_exception_t(QString("Can't open file %1: %2").arg(key).arg(file->errorString()));
  
  return file;
}

std::unique_ptr<QFile> storage_t::tmp_file(const QString& key) const
{
  std::unique_ptr<QFile> file(new QFile(absolute_file_path(key) + storage_t::tmpsuffix));
  if (!file->open(QIODevice::ReadWrite | QIODevice::Truncate)) 
    throw qt_exception_t(QString("Can't open tmp file %1: %2").arg(key).arg(file->errorString()));
  
  return file;
}

void storage_t::fix_tmp_file(std::unique_ptr<QFile>& file, const QString& key) const
{
  if (!file->rename(absolute_file_path(key)))
    throw qt_exception_t(QString("Can't rename file %1 -> %2: %3").arg(file->fileName()).arg(key).arg(file->errorString()));   

  
  file->rename();
}


QFileInfoList storage_t::entries(QString folder) const
{
  QDir dir(prefix() + QDir::separator() + path() + QDir::separator() + folder);
  qDebug() << "mkapth:" << dir.absolutePath();
  dir.mkpath(".");
  if (!dir.exists()) throw qt_exception_t("Folder " + folder + " does not exists");
  
  QFileInfoList result = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden, QDir::DirsFirst);
  result.erase(
    std::remove_if( std::begin(result), std::end(result), [] (const QFileInfo& info) {
      return info.fileName().endsWith(storage_t::tmpsuffix);
    }),
    std::end(result)
  ); 
  
  return result;
}


namespace {
  
const bool self_test = [] () {

  const std::function<void(QString)> rmdir = [&] (QString path) {
    if (QFileInfo(path).isDir()) {
      
      const QDir d(path);
      QDir up(path); up.cdUp();
      
      const auto list = d.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden, QDir::DirsFirst);
      for (int i = 0; i< list.size(); ++i) {
        rmdir(list[i].absoluteFilePath());
      }
      up.rmdir(up.relativeFilePath(d.absolutePath()));
    } else {
      QFile::remove(path);
    }
  };
  
  rmdir("/tmp/davtest1");
  
  {
    qDebug() << "Storage test 1";
    storage_t storage("/tmp/davtest1", "");
    
    Q_ASSERT(storage.root() == "/tmp/davtest1");
    Q_ASSERT(storage.path() == "");
    Q_ASSERT(storage.prefix() == "/tmp/davtest1");
    
    Q_ASSERT(storage.folder("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/file") == "file");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.folder("folder1/folder2/fodler3/") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/") == "");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.absolute_file_path("folder1/folder2/fodler3/file") == "/tmp/davtest1/folder1/folder2/fodler3/file");
    Q_ASSERT(storage.remote_file_path("folder1/folder2/fodler3/file") == "/folder1/folder2/fodler3/file");
    
    ///////////////////////////
    
    const auto names = [] (const QFileInfoList& list) {
      QStringList result;
      for (int i = 0; i < list.size(); ++i) {
        result << list[i].absoluteFilePath();
      };
      return result;
    };
    
    Q_ASSERT(names(storage.entries("")) == QStringList());
    Q_ASSERT(names(storage.entries("/")) == QStringList());
    
    QDir("/").mkpath("/tmp/davtest1/folder1");
    
    Q_ASSERT(names(storage.entries("")) == QStringList() << "/tmp/davtest1/folder1");
    Q_ASSERT(names(storage.entries("/")) == QStringList() << "/tmp/davtest1/folder1");
    
    Q_ASSERT(names(storage.entries("folder1")) == QStringList());
    Q_ASSERT(names(storage.entries("/fodler1")) == QStringList());
    
    QDir("/").mkpath("/tmp/davtest1/folder1/folder2");
    
    Q_ASSERT(names(storage.entries("folder1")) == QStringList() << "/tmp/davtest1/folder1/folder2");
    Q_ASSERT(names(storage.entries("/folder1")) == QStringList() << "/tmp/davtest1/folder1/folder2");
    
    Q_ASSERT(names(storage.entries("fff/ggg")) == names(storage.entries("fff/ggg")));
  }
  
    rmdir("/tmp/davtest1");
  
  {
    qDebug() << "Storage test 2";
    storage_t storage("/tmp/davtest1", "files");
    
    Q_ASSERT(storage.root() == "/tmp/davtest1");
    Q_ASSERT(storage.path() == "files");
    Q_ASSERT(storage.prefix() == "/tmp/davtest1");
    
    
    Q_ASSERT(storage.folder("folder1/folder2//fodler3/file") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/file") == "file");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.folder("folder1/folder2/fodler3/") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/") == "");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");

    
    Q_ASSERT(storage.file_path("files/folder1/folder2/fodler3/file") == "files/folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.absolute_file_path("folder1/folder2/fodler3/file") == "/tmp/davtest1/files/folder1/folder2/fodler3/file");\
    Q_ASSERT(storage.remote_file_path("folder1/folder2/fodler3/file") == "/files/folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.absolute_file_path("files/folder1/folder2/fodler3/file") == "/tmp/davtest1/files/files/folder1/folder2/fodler3/file");
    Q_ASSERT(storage.remote_file_path("files/folder1/folder2/fodler3/file") == "/files/files/folder1/folder2/fodler3/file");
    
    ///////////////////////////
    
    const auto names = [] (const QFileInfoList& list) {
      QStringList result;
      for (int i = 0; i < list.size(); ++i) {
        result << list[i].absoluteFilePath();
      };
      return result;
    };
    
    qDebug() << names(storage.entries(""));
    Q_ASSERT(names(storage.entries("")) == QStringList());
    Q_ASSERT(names(storage.entries("/")) == QStringList());
    
    QDir("/").mkpath("/tmp/davtest1/files/folder1");
    
    Q_ASSERT(names(storage.entries("")) == QStringList() << "/tmp/davtest1/files/folder1");
    Q_ASSERT(names(storage.entries("/")) == QStringList() << "/tmp/davtest1/files/folder1");
    
    Q_ASSERT(names(storage.entries("folder1")) == QStringList());
    Q_ASSERT(names(storage.entries("/fodler1")) == QStringList());
    
    QDir("/").mkpath("/tmp/davtest1/files/folder1/folder2");
    
    Q_ASSERT(names(storage.entries("folder1")) == QStringList() << "/tmp/davtest1/files/folder1/folder2");
    Q_ASSERT(names(storage.entries("/folder1")) == QStringList() << "/tmp/davtest1/files/folder1/folder2");
    
    Q_ASSERT(names(storage.entries("fff/ggg")) == names(storage.entries("fff/ggg")));
  }
  
  return true;
}();

}
