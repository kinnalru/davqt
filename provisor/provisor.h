#ifndef PROVISOR_H
#define PROVISOR_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
//#include <qca.h>

namespace Ui {
class provisor;
}

typedef struct _Group{
    _Group(const QString& _name,/*const QString& _id,*/ const QString& _login, QList<QString>& _campsList){
        name = _name;
        //id = _id;
        login = _login;
        campsList = _campsList;
    }

    QString name,/*id,*/login;
    QList<QString> campsList;
    double rest; //rest in ue
    double speed; //speed of money spent per day
    uint lifetime; // sec
} Group;

typedef struct {

    QString login;
    QString name;
    QString campID;
    QString campName;
} ClientCamp;

class provisor : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit provisor(QWidget *parent = 0);
    ~provisor();
    struct DAK;
    struct campStat;
    QNetworkAccessManager netManager;
    QNetworkReply *networkReply;
    void sendRequest();
    QNetworkRequest request;
    QList<QString> getClientsAndSave();
    QList<Group*> groupSet;
    QTimer timer;
    void getStatAndSave();
    double getRest(QString campsID);
    double getSpentSpeed(QString campID);
    QList<ClientCamp> getCampsAndSave(QList<QString> clientsList);
    int StatPeriod;
    QString getCampNameByID(QString campID);
    QString getClientNameByLogin(QString login);
    void loadTeleData();
    QList<DAK*> kommunist(QList<campStat*> campStatList);

    QNetworkAccessManager testNetManager;
    QNetworkReply *testNetworkReply;
    QNetworkRequest testRequest;
    QUrl url;
    QString Token;
    QString MasterToken;
    QString ApplicationID;
    QVariantList TodayCampsParsedData;
    QVariantList TodayClientsParsedData;
    QVariantList TodayStatParsedData;
    void transfer(QString fromCampId, QString toCampId, double sum);
    uint getNextFinOperationNum(uint currentNum);
    QString getFinToken(QString method, uint finOpNum);
    void loadSettings();
    uint currentFinOpNum;
    QString groupSetPath;
    QString teleDataPath;

public slots:
    void getClientList();
    void slotReadyReadFromReply();
    QVariantMap fillRequestAuthData();
    void replyFinished(QNetworkReply*);
    void createGroup();
    void stopLoop();
    void createGroup_slot();
    void saveGroups();
    void buildReport();
    //void refreshTeleData();
    void expandCollapse_slot();
    void test_slot();
    void slotReadyReadFromTestReply();
    void testReplyFinished(QNetworkReply*);
    void webRedir_slot(QUrl u);
    void saveToken_slot();
    void switshTransfersVisible_slot();
    void callYandexForToken_slot();
    void commentChanged_slot(QString t);
    void refreshStatData();
    void refreshCampsData();

private:
    Ui::provisor *ui;

    QEventLoop loop;
};

#endif // PROVISOR_H
