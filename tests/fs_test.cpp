
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

#include "storage.h"
#include "database/fs/fs.h"

namespace utils {

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

}

struct fs_test {
  fs_test() {
    utils::rmdir("/tmp/davtest1");
    utils::rmdir("/tmp/davtest1.db");
    test1();
    utils::rmdir("/tmp/davtest1");
    utils::rmdir("/tmp/davtest1.db");
    test2();
    utils::rmdir("/tmp/davtest1");
    utils::rmdir("/tmp/davtest1.db");    
    test3();
    utils::rmdir("/tmp/davtest1");
    utils::rmdir("/tmp/davtest1.db");        
  }
  
  void test1()
  {
    qDebug() << "FS Test 1";
    
    storage_t storage("/tmp/davtest1", "", false);
    database::fs_t fs(storage, "/tmp/davtest1.db");
    
    QFile f("/tmp/davtest1/tmp");
    f.open(QIODevice::ReadWrite | QIODevice::Truncate);
    f.write("12345");
    f.close();
    
    QFileInfo info("/tmp/davtest1/tmp");
    
    fs.put("/tmp", fs.create("/tmp", info, info, false));
    auto entry = fs.get("/tmp");

    Q_ASSERT(entry.key == "tmp");
    
    Q_ASSERT(entry.local.lastModified() == entry.remote.lastModified());
    Q_ASSERT(entry.local.lastModified() == info.lastModified());
    Q_ASSERT(entry.local.size() == info.size());
    Q_ASSERT(entry.local.permissions() == info.permissions());
  }
  
  void test2()
  {
    qDebug() << "Test 2";
    
    storage_t storage("/tmp/davtest1", "files", false);
    database::fs_t fs(storage, "/tmp/davtest1/db");
    
    QFile f("/tmp/davtest1/tmp2");
    f.open(QIODevice::ReadWrite | QIODevice::Truncate);
    f.write("12345");
    f.close();
    
    QFileInfo info("/tmp/davtest1/tmp2");
    
    fs.put("/sub1", fs.create("/sub1", info, info, true));
    fs.put("/sub1/sub2/file", fs.create("/sub1/sub2/file", info, info, false));
    auto entry = fs.get("/sub1/sub2/file");

    Q_ASSERT(entry.key == "sub1/sub2/file");
    
    fs.put("files/sub1", fs.create("files/sub1", info, info, true));
    fs.put("files/sub1/sub2/file", fs.create("files/sub1/sub2/file", info, info, false));
    entry = fs.get("files/sub1/sub2/file");

    Q_ASSERT(entry.key == "sub1/sub2/file");
    
    Q_ASSERT(entry.local.lastModified() == entry.remote.lastModified());
    Q_ASSERT(entry.local.lastModified() == info.lastModified());
    Q_ASSERT(entry.local.size() == info.size());
    Q_ASSERT(entry.local.permissions() == info.permissions());
    
    Q_ASSERT(fs.entries().first().key == "sub1");
    Q_ASSERT(fs.entries().last().key == "sub1");
    Q_ASSERT(fs.entries("sub1/sub2").first().key == "sub1/sub2/file");
    
    fs.remove("/sub1/sub2/file");
  }
  
  void test3()
{
    qDebug() << "Test 3";
    
    storage_t storage("/tmp/davtest1", "files", true);
    database::fs_t fs(storage, "/tmp/davtest1/db");
    
    QFile f("/tmp/davtest1/tmp2");
    f.open(QIODevice::ReadWrite | QIODevice::Truncate);
    f.write("12345");
    f.close();
    
    QFileInfo info("/tmp/davtest1/tmp2");
    
    fs.put("/sub1", fs.create("/sub1", info, info, true));
    fs.put("/sub1/sub2/file", fs.create("/sub1/sub2/file", info, info, false));
    auto entry = fs.get("/sub1/sub2/file");

    qDebug() << "key:" << entry.key;
    Q_ASSERT(entry.key == "files/sub1/sub2/file");
    
    fs.put("files/sub1", fs.create("files/sub1", info, info, true));
    fs.put("files/sub1/sub2/file", fs.create("files/sub1/sub2/file", info, info, false));
    entry = fs.get("files/sub1/sub2/file");

    Q_ASSERT(entry.key == "files/sub1/sub2/file");
    
    Q_ASSERT(entry.local.lastModified() == entry.remote.lastModified());
    Q_ASSERT(entry.local.lastModified() == info.lastModified());
    Q_ASSERT(entry.local.size() == info.size());
    Q_ASSERT(entry.local.permissions() == info.permissions());

    Q_ASSERT(fs.entries().first().key == "files/sub1");
    Q_ASSERT(fs.entries().last().key == "files/sub1");
    Q_ASSERT(fs.entries("sub1/sub2").first().key == "files/sub1/sub2/file");
    
    fs.remove("/sub1/sub2/file");
  }
  
};