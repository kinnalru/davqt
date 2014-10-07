
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

#include "storage.h"

namespace {

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

const auto names = [] (const QFileInfoList& list) {
  QStringList result;
  for (int i = 0; i < list.size(); ++i) {
    result << list[i].absoluteFilePath();
  };
  return result;
};

}

struct storage_test {
  storage_test() {
    rmdir("/tmp/davtest1");
    test1();
    rmdir("/tmp/davtest1");
    test2();
    rmdir("/tmp/davtest1");
    test3();
    rmdir("/tmp/davtest1");
  }
  
    
  void test1()
  {
    qDebug() << "Storage test 1";
    storage_t storage("/tmp/davtest1", "", false);
    
    Q_ASSERT(storage.root() == "/tmp/davtest1");
    Q_ASSERT(storage.path() == "");
//     Q_ASSERT(storage.prefix() == "/tmp/davtest1");
    
    Q_ASSERT(storage.folder("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/file") == "file");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.folder("folder1/folder2/fodler3/") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/") == "");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.absolute_file_path("folder1/folder2/fodler3/file") == "/tmp/davtest1/folder1/folder2/fodler3/file");
    Q_ASSERT(storage.remote_file_path("folder1/folder2/fodler3/file") == "/folder1/folder2/fodler3/file");
    
    
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
  
  void test2()
  {
    qDebug() << "Storage test 2";
    storage_t storage("/tmp/davtest1", "files", false);
    
    Q_ASSERT(storage.root() == "/tmp/davtest1");
    Q_ASSERT(storage.path() == "files");
//     Q_ASSERT(storage.prefix() == "/tmp/davtest1/files");
    
    
    Q_ASSERT(storage.folder("folder1/folder2//fodler3/file") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/file") == "file");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.folder("folder1/folder2/fodler3/") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/") == "");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");
    Q_ASSERT(storage.file_path("files/folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");

    Q_ASSERT(storage.absolute_file_path("folder1/folder2/fodler3/file") == "/tmp/davtest1/folder1/folder2/fodler3/file");
    Q_ASSERT(storage.remote_file_path("folder1/folder2/fodler3/file") == "/files/folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.absolute_file_path("files/folder1/folder2/fodler3/file") == "/tmp/davtest1/folder1/folder2/fodler3/file");
    Q_ASSERT(storage.remote_file_path("files/folder1/folder2/fodler3/file") == "/files/folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.absolute_file_path("files/files/folder1/folder2/fodler3/file") == "/tmp/davtest1/files/folder1/folder2/fodler3/file");
    Q_ASSERT(storage.remote_file_path("files/files/folder1/folder2/fodler3/file") == "/files/files/folder1/folder2/fodler3/file");
    

    Q_ASSERT(names(storage.entries("")) == QStringList());
    Q_ASSERT(names(storage.entries("/")) == QStringList());
    Q_ASSERT(names(storage.entries("files")) == QStringList());
    Q_ASSERT(names(storage.entries("/files")) == QStringList());
    
    QDir("/").mkpath("/tmp/davtest1/folder1");

    Q_ASSERT(names(storage.entries("")) == QStringList() << "/tmp/davtest1/folder1");
    Q_ASSERT(names(storage.entries("/")) == QStringList() << "/tmp/davtest1/folder1");
    Q_ASSERT(names(storage.entries("files")) == QStringList() << "/tmp/davtest1/folder1");
    Q_ASSERT(names(storage.entries("/files")) == QStringList() << "/tmp/davtest1/folder1");
    
    Q_ASSERT(names(storage.entries("folder1")) == QStringList());
    Q_ASSERT(names(storage.entries("/fodler1")) == QStringList());
    Q_ASSERT(names(storage.entries("files/fodler1")) == QStringList());
    Q_ASSERT(names(storage.entries("/files/fodler1")) == QStringList());
    
    QDir("/").mkpath("/tmp/davtest1/folder1/folder2");
    
    Q_ASSERT(names(storage.entries("folder1")) == QStringList() << "/tmp/davtest1/folder1/folder2");
    Q_ASSERT(names(storage.entries("/folder1")) == QStringList() << "/tmp/davtest1/folder1/folder2");
    Q_ASSERT(names(storage.entries("files/folder1")) == QStringList() << "/tmp/davtest1/folder1/folder2");
    Q_ASSERT(names(storage.entries("/files/folder1")) == QStringList() << "/tmp/davtest1/folder1/folder2");
    
    Q_ASSERT(names(storage.entries("fff/ggg")) == names(storage.entries("fff/ggg")));
  }
  
  void test3() 
  {
    qDebug() << "Storage test 3";
    storage_t storage("/tmp/davtest1", "files", true);
    
    Q_ASSERT(storage.root() == "/tmp/davtest1");
    Q_ASSERT(storage.path() == "files");
//     Q_ASSERT(storage.prefix() == "/tmp/davtest1/files");
    
    
    Q_ASSERT(storage.folder("folder1/folder2//fodler3/file") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/file") == "file");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.folder("folder1/folder2/fodler3/") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/") == "");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");

    
    Q_ASSERT(storage.file_path("files/folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");

    Q_ASSERT(storage.absolute_file_path("folder1/folder2/fodler3/file") == "/tmp/davtest1/files/folder1/folder2/fodler3/file");
    Q_ASSERT(storage.remote_file_path("folder1/folder2/fodler3/file") == "/files/folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.absolute_file_path("files/folder1/folder2/fodler3/file") == "/tmp/davtest1/files/folder1/folder2/fodler3/file");
    Q_ASSERT(storage.remote_file_path("files/folder1/folder2/fodler3/file") == "/files/folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.absolute_file_path("files/files/folder1/folder2/fodler3/file") == "/tmp/davtest1/files/files/folder1/folder2/fodler3/file");
    Q_ASSERT(storage.remote_file_path("files/files/folder1/folder2/fodler3/file") == "/files/files/folder1/folder2/fodler3/file");
    
    Q_ASSERT(names(storage.entries("")) == QStringList());
    Q_ASSERT(names(storage.entries("/")) == QStringList());
    Q_ASSERT(names(storage.entries("files")) == QStringList());
    Q_ASSERT(names(storage.entries("/files")) == QStringList());
    
    QDir("/").mkpath("/tmp/davtest1/files/folder1");

    Q_ASSERT(names(storage.entries("")) == QStringList() << "/tmp/davtest1/files/folder1");
    Q_ASSERT(names(storage.entries("/")) == QStringList() << "/tmp/davtest1/files/folder1");
    Q_ASSERT(names(storage.entries("files")) == QStringList() << "/tmp/davtest1/files/folder1");
    Q_ASSERT(names(storage.entries("/files")) == QStringList() << "/tmp/davtest1/files/folder1");
    
    Q_ASSERT(names(storage.entries("folder1")) == QStringList());
    Q_ASSERT(names(storage.entries("/fodler1")) == QStringList());
    Q_ASSERT(names(storage.entries("files/fodler1")) == QStringList());
    Q_ASSERT(names(storage.entries("/files/fodler1")) == QStringList());
    
    QDir("/").mkpath("/tmp/davtest1/files/folder1/folder2");
    
    Q_ASSERT(names(storage.entries("folder1")) == QStringList() << "/tmp/davtest1/files/folder1/folder2");
    Q_ASSERT(names(storage.entries("/folder1")) == QStringList() << "/tmp/davtest1/files/folder1/folder2");
    Q_ASSERT(names(storage.entries("files/folder1")) == QStringList() << "/tmp/davtest1/files/folder1/folder2");
    Q_ASSERT(names(storage.entries("/files/folder1")) == QStringList() << "/tmp/davtest1/files/folder1/folder2");
    
    Q_ASSERT(names(storage.entries("fff/ggg")) == names(storage.entries("fff/ggg")));
  }
  
};




