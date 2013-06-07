#include "provisor.h"
#include "ui_provisor.h"

#include <qjson/parser.h>
#include <qjson/serializer.h>
#include <QTextEdit>
#include <QLineEdit>
#include <QSslError>
#include <QSslSocket>
#include <QSslConfiguration>
#include <boost/property_tree/json_parser.hpp>
#include <QEventLoop>
#include <QCheckBox>
#include <QSettings>
#include <QTextCodec>
#include <QFile>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QInputDialog>
#include <QWebView>
#include <limits>
#include <QPalette>

#include "STD_SHA_256.h"

#define CheckBoxColumnNumber 2
#define LoginColumnNumber 0
//#define CampIDColumnNumber 2
#define ButtonColumnNumber 3
#define NameColumnNumber 1
//#define CampNameColumnNumber 1
//#define CampsGroupNameColumnNumber 5

//#define REST_TABLE_LOGIN_COLUMN 4
#define REST_TABLE_GROUP_NAME_COLUMN 0
//#define REST_TABLE_CAMP_NAME_COLUMN 1
#define REST_TABLE_REST_COLUMN 2
#define REST_TABLE_SPEED_COLUMN 3
#define REST_TABLE_LIFETIME_COLUMN 1
#define REST_TABLE_OPTIMAL_COLUMN 5
#define REST_TABLE_POTENTIAL_COLUMN 4
//#define REST_TABLE_SPACING_COLUMN 9
#define REST_TABLE_LINK_COLUMN 6
#define REST_TABLE_COMMENT_COLUMN 9
#define REST_TABLE_COMMSPACING_COLUMN 8
#define REST_TABLE_PLOT_COLUMN 7
#define REST_TABLE_COLUMN_COUNT 10

#define GROUP_SPACING_FONT_SIZE 18
#define CAMP_SPACING_FONT_SIZE 14
#define GROUP_FONT_SIZE 12
#define CAMP_FONT_SIZE 10

#define DATE_STAMP QDate::currentDate().toString("yyyyMMdd")
#define ALARM_TIME 3
#define WARNING_TIME 7
//#define GROUP_SET_INI_FILE_NAME "groupset.ini"

#define WARNING_COLOR QColor(200,0,0)
#define ALARM_COLOR QColor(255,0,0)
#define DEAD_COLOR QColor(100,100,100)
#define TRANSFER_LABEL_COLOR_BG QColor(255,255,170)

#define SETTINGS_FILE_NAME "provisor.ini"

using namespace std;
using namespace QJson;

struct provisor::DAK {
    DAK(const QString& _donor, const QString& _acceptor, const double& _amount){

        donor = _donor;
        acceptor = _acceptor;
        amount = _amount;
    }

    QString donor, acceptor;
    double amount;
};


struct provisor::campStat{
    campStat(const QString& _id, const double& _s,const double& _o){
        s=_s;
        o=_o;
        id = _id;
    }

    double s, o;
    QString id;
};

//namespace boost { namespace property_tree { namespace json_parser
//{
/////Substitute to original boost function to create necessary escape sequences from illegal characters
//template<> std::basic_string<char> create_escapes(const std::basic_string<char> &s);

//}}}


namespace boost { namespace property_tree { namespace json_parser
{
// Create necessary escape sequences from illegal characters
template<>
std::basic_string<char> create_escapes(const std::basic_string<char> &s)
{
    std::basic_string<char> result;
    std::basic_string<char>::const_iterator b = s.begin();
    std::basic_string<char>::const_iterator e = s.end();
    while (b != e)
    {
        if (*b == 0x20 || *b == 0x21 || (*b >= 0x23 && *b <= 0x2E) ||
                (*b >= 0x30 && *b <= 0x5B) || (*b >= 0x5D) // && *b <= 0xFF)  //it fails here because char are signed
                || ( *b < 0 ) ) // && *b >= -0x80 // this will pass UTF-8 signed chars
            result += *b;
        else if (*b == char('\b')) result += char('\\'), result += char('b');
        else if (*b == char('\f')) result += char('\\'), result += char('f');
        else if (*b == char('\n')) result += char('\\'), result += char('n');
        else if (*b == char('\r')) result += char('\\'), result += char('r');
        else if (*b == char('/')) result += char('\\'), result += char('/');
        else if (*b == char('"'))  result += char('\\'), result += char('"');
        else if (*b == char('\\')) result += char('\\'), result += char('\\');
        else
        {
            const char *hexdigits = "0123456789ABCDEF";
            typedef make_unsigned<char>::type UCh;
            unsigned long u = (std::min)(static_cast<unsigned long>(
                                             static_cast<UCh>(*b)),
                                         0xFFFFul);
            int d1 = u / 4096; u -= d1 * 4096;
            int d2 = u / 256; u -= d2 * 256;
            int d3 = u / 16; u -= d3 * 16;
            int d4 = u;
            result += char('\\'); result += char('u');
            result += char(hexdigits[d1]); result += char(hexdigits[d2]);
            result += char(hexdigits[d3]); result += char(hexdigits[d4]);
        }
        ++b;
    }
    return result;
}

}}}


/////////////////////////////////////////////////////////////////////////////////////////////////

QByteArray to_perfect_json(const QVariantMap& map)
{
    //    return QJson::Serializer().serialize(map);
    QByteArray data = QJson::Serializer().serialize(map);
    std::stringstream ss;
    ss.str(data.constData());

    boost::property_tree::ptree ptree;
    boost::property_tree::read_json( ss, ptree );
    boost::property_tree::write_json( ss, ptree );

    return QByteArray(ss.str().c_str());
}
/////////////////////////////////////////////////////////////////////////////////////////////////

QByteArray to_perfect_json(const QByteArray& data)
{
    //    return data;

    std::stringstream ss;
    ss.str(data.constData());

    boost::property_tree::ptree ptree;
    boost::property_tree::read_json( ss, ptree );
    boost::property_tree::write_json( ss, ptree );

    return QByteArray(ss.str().c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

provisor::provisor(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::provisor)
{
    ui->setupUi(this);
    connect(ui->test_btn,SIGNAL(clicked()),this,SLOT(test_slot()));
    connect(ui->getAllClients_btn,SIGNAL(clicked()),this,SLOT(getClientList()));
    connect(ui->saveGroups_btn,SIGNAL(clicked()),this,SLOT(saveGroups()));
    connect(ui->buildReport_btn,SIGNAL(clicked()),this,SLOT(buildReport()));
    connect(ui->refreshStat_btn,SIGNAL(clicked()),this,SLOT(refreshStatData()));
    connect(ui->refreshCamps_btn,SIGNAL(clicked()),this,SLOT(refreshCampsData()));
    connect(&netManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(replyFinished(QNetworkReply*)));
    connect(ui->callYandexForToken_btn,SIGNAL(clicked()),this,SLOT(callYandexForToken_slot()));
    connect(ui->token_btn,SIGNAL(clicked()),this,SLOT(saveToken_slot()));
    connect(ui->showTransfers_btn,SIGNAL(clicked()),this,SLOT(switshTransfersVisible_slot()));
    connect(&testNetManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(testReplyFinished(QNetworkReply*)));
    connect(ui->webView,SIGNAL(linkClicked(QUrl)),SLOT(webRedir_slot(QUrl)));
    connect(ui->webView,SIGNAL(urlChanged(QUrl)),SLOT(webRedir_slot(QUrl)));

    loadSettings();

    QSslConfiguration ssl_conf = request.sslConfiguration();
    ssl_conf.setPeerVerifyMode ( QSslSocket::VerifyNone );
    request.setSslConfiguration(ssl_conf);
    StatPeriod = 7;

    request.setUrl(QUrl("https://api.direct.yandex.ru/live/v4/json/"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json; charset=utf-8");

    //ui->groups_tw->resizeColumnToContents(0);
    //ui->rests_tw->resizeColumnToContents(0);
    ui->rests_tw->clear();
    ui->groups_tw->clear();

    QStringList headers;

    for(int i=0;i<REST_TABLE_COLUMN_COUNT;i++)
    {
        switch(i)
        {
        case REST_TABLE_GROUP_NAME_COLUMN: headers.append(tr("Группа")); break;
        case REST_TABLE_LINK_COLUMN: headers.append(tr("Ссылка")); break;
        case REST_TABLE_OPTIMAL_COLUMN: headers.append(tr("Доп.")); break;
        case REST_TABLE_REST_COLUMN: headers.append(tr("Остаток")); break;
        case REST_TABLE_SPEED_COLUMN: headers.append(tr("Скорость")); break;
        case REST_TABLE_LIFETIME_COLUMN: headers.append(tr("Срок")); break;
        case REST_TABLE_POTENTIAL_COLUMN: headers.append(tr("Возм.")); break;
            //case REST_TABLE_SPACING_COLUMN: headers.append(tr(" ")); break;
        case REST_TABLE_COMMSPACING_COLUMN: headers.append(tr(" ")); break;
        case REST_TABLE_COMMENT_COLUMN: headers.append(tr("Комментарий")); break;
        case REST_TABLE_PLOT_COLUMN: headers.append(tr("Графики")); break;
        default: headers.append(tr(" "));
        }
    }

    ui->rests_tw->setHeaderLabels ( headers )    ;

    for(int i=1;i<ui->rests_tw->headerItem()->columnCount();i++)
    {
        ui->rests_tw->headerItem()->setTextAlignment(i,Qt::AlignRight|Qt::AlignVCenter);
    }

    saveToken_slot();
    ui->transfer_te->hide();
    ui->mainToolBar->hide();
    ui->menuBar->hide();
    ui->statusBar->showMessage(tr("Готов к труду и обороне!"),5000);
    ui->buildReport_btn->setFocus();
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

provisor::~provisor()
{
    delete ui;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::getClientList()
{
    QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );

    QList<QString> clients = getClientsAndSave();
    QList<ClientCamp> cclist = getCampsAndSave(clients);
    QString prevLogin="";
    ui->groups_tw->clear();

    foreach(ClientCamp cc, cclist)
    {
        if((prevLogin.compare(cc.login)!=0))
        {
            QTreeWidgetItem* clientItem = new QTreeWidgetItem();
            clientItem->setText(LoginColumnNumber,cc.login);
            clientItem->setText(NameColumnNumber,cc.name);
            ui->groups_tw->insertTopLevelItem(ui->groups_tw->topLevelItemCount(),clientItem);
            ui->groups_tw->setItemWidget(clientItem,ButtonColumnNumber,new QPushButton(tr("Создать группу")));
            ((QPushButton*)(ui->groups_tw->itemWidget(clientItem,ButtonColumnNumber)))->setMaximumSize(120,30);
            connect(((QPushButton*)(ui->groups_tw->itemWidget(clientItem,ButtonColumnNumber))),SIGNAL(clicked()),this,SLOT(createGroup_slot()));
            ui->groups_tw->setCurrentItem(clientItem);
        }

        prevLogin = cc.login;
        QTreeWidgetItem* campItem = new QTreeWidgetItem();
        campItem->setText(LoginColumnNumber,cc.campName);
        campItem->setText(NameColumnNumber,cc.campID);
        ui->groups_tw->currentItem()->addChild(campItem);
        ui->groups_tw->setItemWidget(campItem,CheckBoxColumnNumber,new QCheckBox);

        //  connect(((QCheckBox*)(ui->tableWidget->cellWidget(row,CheckBoxColumnNumber))),SIGNAL(stateChanged(int)),this,SLOT(disableOtherLogins(int)));
    }
    ui->groups_tw->resizeColumnToContents(LoginColumnNumber);
    ui->groups_tw->resizeColumnToContents(NameColumnNumber);
    ui->groups_tw->resizeColumnToContents(CheckBoxColumnNumber);
    ui->groups_tw->resizeColumnToContents(ButtonColumnNumber);

    QApplication::restoreOverrideCursor();

}

QVariantMap provisor::fillRequestAuthData()
{
    QVariantMap auth;

    auth.insert("locale","ru");
    auth.insert("application_id",ApplicationID);
    auth.insert("token",Token);
    auth.insert("login",ui->login_te->text());

    return auth;

}

/////////////////////////////////////////////////////////////////////////////////////////////////

void provisor::slotReadyReadFromReply()
{
    cout << "Entering into slotReadyReadFromReply()" << endl;
    //    loop.exit();

    //    cout << m["SumSearch"].toString().toStdString() << endl;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void provisor::replyFinished(QNetworkReply*)
{
    loop.exit();
    cout << "Entering into replyFinished()" << endl;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
QList<ClientCamp> provisor::getCampsAndSave(QList<QString> clientsList)
{

    cout << "Entering into getCamps()" << endl;

    QByteArray json;
    QFile f;
    f.setFileName(QString("%1camps%2.json").arg(teleDataPath).arg(DATE_STAMP));
    QList<ClientCamp> clientCampList;
    ClientCamp clientCamp;

    if(!f.exists())
    {
        QVariantList logins;
        foreach(QString cl, clientsList)
        {
            logins.append(cl);
        }

        //QVariantMap camps;
        QVariantMap getCampsRequest = fillRequestAuthData();
        getCampsRequest.insert("param", logins);
        getCampsRequest.insert("method", "GetCampaignsList");

        Serializer serializer;
        QByteArray jsonRequest;

        jsonRequest = serializer.serialize(getCampsRequest);

        networkReply = netManager.post(request,jsonRequest);
        connect(networkReply, SIGNAL(readyRead()), this, SLOT(slotReadyReadFromReply()));

        ui->statusBar->showMessage(tr("Запрос кампаний с сервера..."),15000);

        QTimer::singleShot(15000, this, SLOT(stopLoop()));
        loop.exec();

        //*********************************************************

        json = networkReply->readAll();
        ui->statusBar->showMessage(QString(tr("Данные кампаний получены. Размер: %1.").arg(QString().setNum(json.size()))),15000);

        f.open(QIODevice::WriteOnly);
        f.write(to_perfect_json(json));
        f.close();
    }
    else
    {
        f.open(QIODevice::ReadOnly);
        json = f.readAll();
        //     ui->statusBar->showMessage(QString(tr("Данные кампаний загружены. Размер: %1.").arg(QString().setNum(json.size()))),15000);
        f.close();
    }

    Parser parser;
    QVariantMap info = parser.parse(json, 0).toMap();
    TodayCampsParsedData = info["data"].toList();

    foreach (QVariant d, TodayCampsParsedData)
    {
        if(d.toMap()["StatusArchive"].toString().compare("Yes")==0)
            continue;

        clientCamp.campID = d.toMap()["CampaignID"].toString();
        clientCamp.campName = QString().fromUtf8(d.toMap()["Name"].toString().toStdString().data());
        clientCamp.login = d.toMap()["Login"].toString();
        clientCamp.name = getClientNameByLogin(d.toMap()["Login"].toString());
        clientCampList.append(clientCamp);
    }

    //    ui->statusBar->showMessage(QString(tr("Кампании загружены. Всего %1 элементов.").arg(QString().setNum(TodayCampsParsedData.count()))),10000);

    return clientCampList;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
QList<QString> provisor::getClientsAndSave()
{
    cout << "Entering into getClientsList()" << endl;
    QByteArray json;
    QFile f;
    f.setFileName(QString("%1clients%2.json").arg(teleDataPath).arg(DATE_STAMP));
    //QList<QString> clientsList;

    if(!f.exists())
    {

        QVariantMap filter;
        QVariantMap statArch;

        statArch.insert("StatusArch","No");
        filter.insert("Filter",statArch);

        QVariantMap getClientsRequest = fillRequestAuthData();
        getClientsRequest.insert("param", filter);
        getClientsRequest.insert("method", "GetClientsList");

        Serializer serializer;
        QByteArray jsonRequest;

        jsonRequest = serializer.serialize(getClientsRequest);

        networkReply = netManager.post(request,jsonRequest);

        connect(networkReply, SIGNAL(readyRead()), this, SLOT(slotReadyReadFromReply()));

        QTimer::singleShot(15000, this, SLOT(stopLoop()));
        loop.exec();

        //*********************************************************

        json = networkReply->readAll();
        f.open(QIODevice::WriteOnly);
        f.write(to_perfect_json(json));
        f.close();
    }
    else
    {
        f.open(QIODevice::ReadOnly);
        json = f.readAll();
        f.close();
    }


    Parser parser;
    QVariantMap info = parser.parse (json, 0).toMap();
    TodayClientsParsedData = info["data"].toList();

    QStringList clientsList;

    foreach (QVariant d, TodayClientsParsedData) {
        clientsList.append(d.toMap()["Login"].toString());
    }

    return clientsList;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::createGroup_slot()
{
    createGroup();
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::saveGroups()
{

    QSettings gSet(groupSetPath,QSettings::IniFormat);
    gSet.setIniCodec(QTextCodec::codecForName("utf8"));

    uint n=0;
    uint k=0;

    foreach(Group* g, groupSet)
    {
        //        cout << "**********************************" << endl;
        //        cout << "Group name:" << g->name.toStdString().data() << endl;
        //        cout << "Group client:" << g->login.toStdString().data() << endl;

        gSet.beginGroup(QString().setNum(n));
        gSet.setValue("name",g->name);
        gSet.setValue("login",g->login);
        //gSet.setValue("rest",getRests(g->campsList));

        //        cout << "Group camps IDs:";

        foreach(QString s, g->campsList)
        {
            //            cout << s.toStdString().data() << " ";
            gSet.setValue( QString().setNum(k), s);
            k++;
        }
        //        cout << endl;
        k=0;
        n++;
        gSet.endGroup();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////


void provisor::createGroup()
{
    QList<QString> campsIDsList;
    QString login;
    QString campID;
    QString groupName;


    for(int client=0;client<ui->groups_tw->topLevelItemCount();client++)
    {
        for(int camp=0;camp<ui->groups_tw->topLevelItem(client)->childCount();camp++)
        {
            if(((QCheckBox*)(ui->groups_tw->itemWidget(ui->groups_tw->topLevelItem(client)->child(camp),CheckBoxColumnNumber)))->isChecked())
            {
                campsIDsList.append(ui->groups_tw->topLevelItem(client)->child(camp)->text(NameColumnNumber));
                ((QCheckBox*)(ui->groups_tw->itemWidget(ui->groups_tw->topLevelItem(client)->child(camp),CheckBoxColumnNumber)))->setChecked(false);
            }
        }
        if(campsIDsList.isEmpty())
            continue;

        groupName = QInputDialog::getText(this, tr("Название группы"),
                                          tr("Название группы:"), QLineEdit::Normal,
                                          ui->groups_tw->topLevelItem(client)->text(NameColumnNumber), NULL);

        login = ui->groups_tw->topLevelItem(client)->text(LoginColumnNumber);
        groupSet.append(new Group(groupName,login,campsIDsList));
        //groupSet.append(new Group(login,login,campsIDsList));
        break;
        //campsIDsList.clear();
    };
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::stopLoop()
{
    loop.exit();
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void provisor::buildReport()
{
    cout << "Enter in refreshReport()" << endl;

    QApplication::setOverrideCursor( QCursor(Qt::WaitCursor) );


    if(!QFile::exists(groupSetPath))
    {
        cerr << "File with groupset was not found" << endl;
        return;
    }

    loadTeleData();

    QSettings gSet(groupSetPath,QSettings::IniFormat);
    gSet.setIniCodec(QTextCodec::codecForName("utf8"));
    gSet.sync();
    QStringList gSetStr = gSet.childGroups();

    ui->rests_tw->clear();
    ui->transfer_te->clear();

    uint k=0;
    // QTableWidgetItem* item;
    groupSet.clear();
    QString campID;
    double minLifeTime=numeric_limits<double>::max();
    double potentialLifeTime=0;
    double groupRest=0;
    double groupSpeed=0;
    double rest = 0;
    double speed = 0;
    double lifetime = 0;
    QTreeWidgetItem* campItem;
    //QTreeWidgetItem* trItem;
    QFont font;
    QList<campStat*> campStatList;
    QList<DAK*> DAKList;
    //QLabel *lab;
    QString tr="";
    QPalette palette;
    palette.setColor(QPalette::Window,TRANSFER_LABEL_COLOR_BG);
    QString plotUrl;
    QFrame* transFrame;
    //QTextEdit* comment_te;

    ui->label_2->setAutoFillBackground(true);
    ui->label_2->setPalette(palette);

    QDate yesterday = QDate::currentDate();
    yesterday = yesterday.addDays(-1);

    QDate dateStatPeriod = QDate::currentDate();
    dateStatPeriod = dateStatPeriod.addDays(-1*(StatPeriod));

    QLabel* lab;
    QPushButton* trans_btn;

    foreach(QString groupId, gSetStr)
    {
        gSet.beginGroup(groupId);

        //создаем и добавляем элемент верхнего уровня "Группа Кампаний"
        QTreeWidgetItem* groupItem = new QTreeWidgetItem();
        groupItem->setText(REST_TABLE_GROUP_NAME_COLUMN,gSet.value("name").toString());
        ui->rests_tw->insertTopLevelItem(ui->rests_tw->topLevelItemCount(),groupItem);
        ui->rests_tw->setCurrentItem(groupItem);
        // ------------ Визуальные улучшения ---------------- //
        font.setPointSize(GROUP_FONT_SIZE);
        font.setBold(false);

        for(int i=0;i<ui->rests_tw->columnCount();i++)
        {
            ui->rests_tw->currentItem()->setFont(i,font);
            if(i>0)
                ui->rests_tw->currentItem()->setTextAlignment(i,Qt::AlignRight|Qt::AlignVCenter);
        }//for

        font.setPointSize(GROUP_SPACING_FONT_SIZE);// large font for spacing

        ui->rests_tw->currentItem()->setText(REST_TABLE_COMMSPACING_COLUMN," ");
        ui->rests_tw->currentItem()->setFont(REST_TABLE_COMMSPACING_COLUMN,font);

        font.setPointSize(GROUP_FONT_SIZE);

        //--------------------------------------------------------//
        campStatList.clear();

        //добавляем вложенные элементы с информацией по каждой кампании из группы
        while(gSet.contains(QString().setNum(k)))
        {
            // берем ID кампании и получаем остаток денег и скорость расхода
            campID = gSet.value(QString().setNum(k)).toString();
            rest = getRest(campID);
            speed = getSpentSpeed(campID);
            campItem = new QTreeWidgetItem();

            campStatList.append(new campStat(campID,speed,rest));

            //если скорость не ноль - то рассчитать прогноз
            if(speed>0)
            {
                lifetime = rest/speed;
                campItem->setText(REST_TABLE_LIFETIME_COLUMN,QString().setNum(lifetime,'f',1));
            }
            //если скорость = 0, то заполнить прогноз бесконечностью и закрасить серым
            else
            {
                campItem->setText(REST_TABLE_LIFETIME_COLUMN,"\u2014");
                campItem->setTextColor(REST_TABLE_LIFETIME_COLUMN,DEAD_COLOR);
                lifetime = numeric_limits<double>::max();
            }

            // определение кампании с наименьшим временем жизни
            if(minLifeTime>lifetime)
                minLifeTime = lifetime;

            // собираем скорость и остаток для группы кампаний
            groupRest+=rest;
            groupSpeed+=speed;

            //заполняем ячейки конкретной кампании всякими данными
            campItem->setText(REST_TABLE_GROUP_NAME_COLUMN,getCampNameByID(gSet.value(QString().setNum(k)).toString()));
            campItem->setText(REST_TABLE_REST_COLUMN,QString().setNum(rest,'f',2)/*+tr(" уе")*/);
            campItem->setText(REST_TABLE_SPEED_COLUMN,QString().setNum(speed,'f',2));

            font.setPointSize(CAMP_FONT_SIZE);
            for(int i=0;i<campItem->columnCount();i++)
            {
                campItem->setFont(i,font);
                if(i>0)
                    campItem->setTextAlignment(i,Qt::AlignRight|Qt::AlignVCenter);
            }//for
b
            font.setBold(true);
            if(lifetime<=ALARM_TIME)
            {
                campItem->setTextColor(REST_TABLE_LIFETIME_COLUMN,ALARM_COLOR);
                campItem->setFont(REST_TABLE_LIFETIME_COLUMN,font);
            }
            else if(lifetime<=WARNING_TIME)
            {
                campItem->setTextColor(REST_TABLE_LIFETIME_COLUMN,WARNING_COLOR);
                campItem->setFont(REST_TABLE_LIFETIME_COLUMN,font);
            }
            font.setBold(false);

            font.setPointSize(CAMP_SPACING_FONT_SIZE);
            campItem->setFont(REST_TABLE_COMMSPACING_COLUMN,font);
            font.setPointSize(CAMP_FONT_SIZE);

            // добавляем в дерево группы строку с кампанией
            ui->rests_tw->currentItem()->addChild(campItem);

            plotUrl = QString("https://direct.yandex.ru/registered/main.pl?y1=%1&m1=%2&d1=%3&y2=%4&m2=%5&d2=%6&group=day&group_by=date&ulogin=%7&plot_data=&target_all=yes&cid=%8&plotcol=sum&cmd=showPlotPage&plot=1&norefresh=0")
                    .arg(dateStatPeriod.year())
                    .arg(dateStatPeriod.month())
                    .arg(dateStatPeriod.day())
                    .arg(yesterday.year())
                    .arg(yesterday.month())
                    .arg(yesterday.day())
                    .arg(gSet.value("login").toString())
                    .arg(gSet.value(QString().setNum(k)).toString());

            ui->rests_tw->setItemWidget(campItem,REST_TABLE_PLOT_COLUMN,new QLabel(QString("<a href=\"%1\">График</a>").arg(plotUrl)));
            ((QLabel *)(ui->rests_tw->itemWidget(campItem,REST_TABLE_PLOT_COLUMN)))->setOpenExternalLinks(true);
            ((QLabel *)(ui->rests_tw->itemWidget(campItem,REST_TABLE_PLOT_COLUMN)))->setAlignment(Qt::AlignRight|Qt::AlignVCenter);

            k++;
        }//По каждой Кампании из Группы

        //расчет возможных переносов, если некоторые кампании скоро закончатся
        if((campStatList.count()>1) && (minLifeTime<=WARNING_TIME))
        //if((campStatList.count()>1))
        {
            DAKList = kommunist(campStatList);
            if(!DAKList.isEmpty())
            {
                ui->transfer_te->append(QString("<b> %1 </b>").arg(gSet.value("name").toString()));

                campItem = new QTreeWidgetItem();
                transFrame = new QFrame();
                transFrame->setAutoFillBackground(true);
                transFrame->setFrameShape(QFrame::Box);
                transFrame->setFrameShadow(QFrame::Raised);

                int i;
                for(i=0;i<DAKList.size();i++){
                    ui->transfer_te->append(getCampNameByID(DAKList.at(i)->donor) + " → " + getCampNameByID(DAKList.at(i)->acceptor) + ": " + QString().setNum(DAKList.at(i)->amount,'f',2));
                    lab = new QLabel(QString(getCampNameByID(DAKList.at(i)->donor) + " → " + getCampNameByID(DAKList.at(i)->acceptor) + ": " + QString().setNum(DAKList.at(i)->amount,'f',2)),transFrame);
                    lab->move(5,5+20*i);
                }

                trans_btn = new QPushButton("Распределить",transFrame);
                trans_btn->move(5,5+20*i);

                lab = new QLabel(QString("<a href=\"https://direct.yandex.ru/registered/main.pl?cmd=transfer&ulogin=%1\">Перенос средств</a>").arg(gSet.value("login").toString()),transFrame);;
                lab->setOpenExternalLinks(true);
                lab->move(trans_btn->width(),8+20*i);

                transFrame->setMinimumSize(0,5+20*i+trans_btn->height());

                ui->rests_tw->currentItem()->addChild(campItem);
                ui->rests_tw->setItemWidget(campItem,REST_TABLE_GROUP_NAME_COLUMN,transFrame);

                ui->transfer_te->append(QString("<a href=\"https://direct.yandex.ru/registered/main.pl?cmd=transfer&ulogin=%1\">Перенос средств</a>").arg(gSet.value("login").toString()));
                ui->transfer_te->append("");
            }//если есть что и куда перенести
        }//если можно и нужно переносить
        ui->transfer_te->setFont(font);

        //если скорость группы не ноль - то рассчитать прогноз
        if(groupSpeed>0 && k>1 && !DAKList.isEmpty() && (minLifeTime<=WARNING_TIME))
//        if(groupSpeed>0 && k>1 && !DAKList.isEmpty())
        {
            potentialLifeTime=groupRest/groupSpeed;
            ui->rests_tw->currentItem()->setText(REST_TABLE_POTENTIAL_COLUMN,QString().setNum(potentialLifeTime,'f',1));
            ui->rests_tw->currentItem()->setText(REST_TABLE_OPTIMAL_COLUMN,QString().setNum(potentialLifeTime-minLifeTime,'f',1));
        }

        if(k==1)
            ui->rests_tw->currentItem()->removeChild(campItem);

        //заполняем скорость и остаток для группы
        ui->rests_tw->currentItem()->setText(REST_TABLE_REST_COLUMN,QString().setNum(groupRest,'f',2)/*+tr(" уе")*/);
        ui->rests_tw->currentItem()->setText(REST_TABLE_SPEED_COLUMN,QString().setNum(groupSpeed,'f',2));

        //разбор значения минимального времени жизни кампаний
        if(minLifeTime < numeric_limits<double>::max())
        {
            ui->rests_tw->currentItem()->setText(REST_TABLE_LIFETIME_COLUMN,QString().setNum(minLifeTime,'f',1));
            font.setBold(true);
            font.setPointSize(GROUP_FONT_SIZE);
            if(minLifeTime<=ALARM_TIME)
            {
                ui->rests_tw->currentItem()->setTextColor(REST_TABLE_LIFETIME_COLUMN,ALARM_COLOR);
                ui->rests_tw->currentItem()->setFont(REST_TABLE_LIFETIME_COLUMN,font);
            }
            else if (minLifeTime<=WARNING_TIME)
            {
                ui->rests_tw->currentItem()->setTextColor(REST_TABLE_LIFETIME_COLUMN,WARNING_COLOR);
                ui->rests_tw->currentItem()->setFont(REST_TABLE_LIFETIME_COLUMN,font);
            }
        }
        else
        {
            font.setBold(true);
            font.setPointSize(GROUP_FONT_SIZE);

            ui->rests_tw->currentItem()->setText(REST_TABLE_LIFETIME_COLUMN,QString("\u2014"));
            ui->rests_tw->currentItem()->setTextColor(REST_TABLE_LIFETIME_COLUMN,DEAD_COLOR);
            ui->rests_tw->currentItem()->setFont(REST_TABLE_LIFETIME_COLUMN,font);
        }

        ui->rests_tw->setItemWidget(ui->rests_tw->currentItem(),REST_TABLE_LINK_COLUMN,new QLabel(QString("<a href=\"https://direct.yandex.ru/registered/main.pl?cmd=showCamps&ulogin=%1\">Открыть</a>").arg(gSet.value("login").toString())));
        ((QLabel *)(ui->rests_tw->itemWidget(ui->rests_tw->currentItem(),REST_TABLE_LINK_COLUMN)))->setOpenExternalLinks(true);
        ((QLabel *)(ui->rests_tw->itemWidget(ui->rests_tw->currentItem(),REST_TABLE_LINK_COLUMN)))->setAlignment(Qt::AlignRight|Qt::AlignVCenter);

        plotUrl = QString("https://direct.yandex.ru/registered/main.pl?y1=%1&m1=%2&d1=%3&y2=%4&m2=%5&d2=%6&group=day&group_by=date&norefresh=1&ulogin=%7&plot_data=&target_all=yes&plotcol=sum&cmd=showPlotPage&plot=1&stat_type=campdate&norefresh=1")
                .arg(dateStatPeriod.year())
                .arg(dateStatPeriod.month())
                .arg(dateStatPeriod.day())
                .arg(yesterday.year())
                .arg(yesterday.month())
                .arg(yesterday.day())
                .arg(gSet.value("login").toString());

        ui->rests_tw->setItemWidget(ui->rests_tw->currentItem(),REST_TABLE_PLOT_COLUMN,new QLabel(QString("<a href=\"%1\">График</a>").arg(plotUrl)));
        ((QLabel *)(ui->rests_tw->itemWidget(ui->rests_tw->currentItem(),REST_TABLE_PLOT_COLUMN)))->setOpenExternalLinks(true);
        ((QLabel *)(ui->rests_tw->itemWidget(ui->rests_tw->currentItem(),REST_TABLE_PLOT_COLUMN)))->setAlignment(Qt::AlignRight|Qt::AlignVCenter);

        ui->rests_tw->setItemWidget(ui->rests_tw->currentItem(),REST_TABLE_COMMENT_COLUMN,new QLineEdit(gSet.value("comment").toString()));
        connect((QLineEdit *)(ui->rests_tw->itemWidget(ui->rests_tw->currentItem(),REST_TABLE_COMMENT_COLUMN)),SIGNAL(textChanged(QString)),SLOT(commentChanged_slot(QString)));
        ((QLineEdit *)(ui->rests_tw->itemWidget(ui->rests_tw->currentItem(),REST_TABLE_COMMENT_COLUMN)))->setFrame(false);
        ((QLineEdit *)(ui->rests_tw->itemWidget(ui->rests_tw->currentItem(),REST_TABLE_COMMENT_COLUMN)))->setObjectName(groupId);

        minLifeTime=numeric_limits<double>::max();
        groupRest=0;
        groupSpeed=0;
        k=0;
        gSet.endGroup();
    }//цикл по всем Группам в ini файле

    ui->rests_tw->collapseAll();
    ui->rests_tw->sortByColumn(REST_TABLE_GROUP_NAME_COLUMN,Qt::AscendingOrder);
    ui->rests_tw->hideColumn(REST_TABLE_POTENTIAL_COLUMN);

    for(int i=0;i<ui->rests_tw->columnCount();i++)
        ui->rests_tw->resizeColumnToContents(i);

    ui->rests_tw->setColumnWidth(REST_TABLE_COMMSPACING_COLUMN,15);

    QApplication::restoreOverrideCursor();
}

/////////////////////////////////////////////////////////////////////////////////////////////////
double provisor::getRest(QString campID)
{

    if(TodayCampsParsedData.isEmpty())
    {
        cerr << "TodayCampsParsedData is Empty" << endl;
        return -1;
    }

    foreach (QVariant d, TodayCampsParsedData)
    {
        if(d.toMap()["CampaignID"].toString().compare(campID)==0)
        {
            return d.toMap()["Rest"].toDouble();
        }
    }//by all IDs in data

    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::getStatAndSave()
{
    cout << "getStatAndSave()" << endl;

    //  QStringList campsIDs;

    if(!QFile::exists(QString(groupSetPath)))
    {
        cerr << "can't find groups file: " << QString(groupSetPath).toStdString().data() << endl;
        return;
    }

    QByteArray json;
    QFile f;
    f.setFileName(QString("%1stat%2.json").arg(teleDataPath).arg(DATE_STAMP));
    QSettings gSet(groupSetPath,QSettings::IniFormat);
    gSet.setIniCodec(QTextCodec::codecForName("utf8"));
    gSet.sync();
    QStringList gSetStr = gSet.childGroups();
    int k=0;
    Parser parser;
    QVariantMap info;

    QString message="";

    if(!f.exists())
    {
        QVariantList camps;
        foreach(QString s, gSetStr)
        {
            gSet.beginGroup(s);
            while(gSet.contains(QString().setNum(k)))
            {
                camps.append(gSet.value(QString().setNum(k)).toString());
                k++;
            }
            k=0;
            gSet.endGroup();
        }

        QDate dt = QDate::currentDate();
        dt = dt.addDays(-1);
        QString yesterday = dt.toString(Qt::ISODate);
        dt = dt.addDays(-1*(StatPeriod-1));
        QString prevweek = dt.toString(Qt::ISODate);

        Serializer serializer;
        QByteArray jsonRequest;

        int iteration_count = 1000/StatPeriod;

        QVariantList camps_it;
        TodayStatParsedData.clear();
        for(int i=0;i<camps.count()/iteration_count+1;i++)
        {
            camps_it.clear();
            camps_it.append(camps.mid(i*iteration_count,iteration_count));
            QVariantMap param;
            param.insert("CampaignIDS",camps_it);
            param.insert("StartDate", prevweek);
            param.insert("EndDate", yesterday);
            QVariantMap getStatRequest = fillRequestAuthData();
            getStatRequest.insert("param", param);
            getStatRequest.insert("method", "GetSummaryStat");
            jsonRequest = serializer.serialize(getStatRequest);
            networkReply = netManager.post(request,jsonRequest);
            connect(networkReply, SIGNAL(readyRead()), this, SLOT(slotReadyReadFromReply()));
            QTimer::singleShot(15000, this, SLOT(stopLoop()));

            message+=QString(tr("Запрос отправлен... "));
            ui->statusBar->showMessage(message,20000);
            loop.exec();
            message+=QString(tr("Ответ получен. "));
            ui->statusBar->showMessage(message,20000);
            //*********************************************************
            json = networkReply->readAll();

            message+=QString(tr("Итерация №%2. Данных получено: %1 ")).arg(QString().setNum(json.size())).arg(i+1);
            ui->statusBar->showMessage(message,20000);

            info = parser.parse (json, 0).toMap();
            TodayStatParsedData.append(info["data"].toList());
            //         cout << "TodayStatParsedData.count: " << TodayStatParsedData.count() << endl;
        }
        QVariantMap stat;
        stat.insert("data",TodayStatParsedData);
        QByteArray jsonStat = serializer.serialize(stat);

        f.open(QIODevice::WriteOnly);
        f.write(to_perfect_json(jsonStat));
    }
    else
    {
        //-----------------Load Stat ------------------//
        f.open(QIODevice::ReadOnly);
        json = f.readAll();

        info = parser.parse (json, 0).toMap();
        TodayStatParsedData = info["data"].toList();
    }

    f.close();
    //    ui->statusBar->showMessage(QString(tr("Статистика загружена. Всего %1 элементов.").arg(QString().setNum(TodayStatParsedData.count()))),5000);
}


/////////////////////////////////////////////////////////////////////////////////////////////////
double provisor::getSpentSpeed(QString campsID)
{
    double sum = 0;
    int days=0;

    foreach (QVariant d, TodayStatParsedData)
    {
        if(d.toMap()["CampaignID"].toString().compare(campsID)==0)
        {
            sum+=d.toMap()["SumSearch"].toDouble()+ d.toMap()["SumContext"].toDouble();
            days++;
        }
    }//by all in data

    if(days == 0)
        return 0;

    return sum/days;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void provisor::expandCollapse_slot()
{

    if(ui->rests_tw->isExpanded(ui->rests_tw->model()->index(0,0)))
        ui->rests_tw->collapseAll();
    else
        ui->rests_tw->expandAll();

}

/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::loadTeleData()
{
    QList<QString> clients = getClientsAndSave();
    getCampsAndSave(clients);
    getStatAndSave();
}

/////////////////////////////////////////////////////////////////////////////////////////////////

QString provisor::getCampNameByID(QString campID)
{
    if(TodayCampsParsedData.isEmpty())
    {
        cerr << "TodayCampsParsedData is Empty" << endl;
        return "";
    }

    foreach (QVariant d, TodayCampsParsedData)
    {
        if(d.toMap()["CampaignID"].toString().compare(campID)==0)
        {
            //            return d.toMap()["Name"].toString();
            return QString().fromUtf8(d.toMap()["Name"].toString().toStdString().data());

        }
    }//by all IDs in data
    return "";
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::callYandexForToken_slot()
{
    url = QUrl(ui->url_le->text());
    testRequest.setUrl(url);
    testRequest.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");

    QSslConfiguration ssl_conf = testRequest.sslConfiguration();
    ssl_conf.setPeerVerifyMode ( QSslSocket::VerifyNone );
    testRequest.setSslConfiguration(ssl_conf);

    testNetworkReply = testNetManager.post(testRequest,"");

    connect(testNetworkReply, SIGNAL(readyRead()), this, SLOT(slotReadyReadFromTestReply()));

    QTimer::singleShot(15000, this, SLOT(stopLoop()));
    loop.exec();

    //*********************************************************

    QByteArray rep = testNetworkReply->readAll();

    ui->txt->append("rep.size: " + QString().setNum(rep.size()));

}

/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::slotReadyReadFromTestReply()
{
    cout << "slotReadyReadFromTestReply" << endl;
    ui->txt->append("slotReadyReadFromTestReply");
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::testReplyFinished(QNetworkReply*)
{

    loop.exit();
    cout << "testReplyFinished" << endl;
    ui->txt->append("testReplyFinished");

    QVariant redirectionTarget = testNetworkReply->attribute(QNetworkRequest::RedirectionTargetAttribute);

    /*  if (testNetworkReply->error()) {

        ui->txt->append("testNetworkReply->error(): "+testNetworkReply->errorString());

    } else if (!redirectionTarget.isNull()) {*/
    QUrl newUrl = url.resolved(redirectionTarget.toUrl());
    ui->txt->append("New url: " + newUrl.toString());

    ui->webView->load(newUrl);
    ui->webView->show();

    //        return;
    //};
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void provisor::webRedir_slot(QUrl u)
{
    ui->txt->append(u.toString());
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void provisor::saveToken_slot()
{

    Token = ui->token_le->text();
    MasterToken = ui->masterToken_le->text();
    ApplicationID = ui->appID_le->text();

    request.setRawHeader(QByteArray("Authorization"),QByteArray(QString("OAuth %1").arg(Token).toStdString().data()));

}
/////////////////////////////////////////////////////////////////////////////////////////////////

QString provisor::getClientNameByLogin(QString login)
{
    foreach (QVariant d, TodayClientsParsedData) {
        if(d.toMap()["Login"].toString().compare(login)==0)
            return d.toMap()["FIO"].toString();
    }

    return "";
}
/////////////////////////////////////////////////////////////////////////////////////////////////

QList<provisor::DAK*> provisor::kommunist(QList<campStat*> campStatList)
{

    QList<provisor::DAK*> DAKList;

    int  N=campStatList.size();

    double s[N];
    double o[N];

    double t[N];
    double d[N];
    double p[N];

    int i;
    double total_t=0.0;
    double total_s=0.0,total_o=0.0;
    double minLifeTime=numeric_limits<double>::max();

    for(i=0;i<N;i++){
        s[i]=campStatList.at(i)->s;
        o[i]=campStatList.at(i)->o;
    }

    for(i=0;i<N;i++)
    {
        t[i] = (double)o[i]/(double)s[i];
        if(t[i]<minLifeTime)
            minLifeTime = t[i];
        total_o+=o[i];
        total_s+=s[i];
    }
    //   ui->transfer_te->append("minLifeTime: " + QString().setNum(minLifeTime));

    total_t = (double)total_o/(double)total_s;

    //  ui->transfer_te->append("total_T: " + QString().setNum(total_t));

    //Вычисление дефицитов и потенциалов
    for(i=0;i<N;i++)
    {
        d[i] = (total_t - t[i])*(double)s[i];
        p[i] = (t[i] - total_t)*(double)s[i];

        if(d[i]<5) d[i]=0;
        else if(d[i]<10) d[i]=10;

        if(o[i]>=20 && d[i]==0)
        {
            if(s[i]*total_t > 10)
                p[i] = o[i]-s[i]*total_t;
            else
                p[i] = o[i]-10; //потому что нельзя оставить на кампании меньше 10 уе

            if(p[i]<10) p[i] = 0; // потому что нельзя перенести меньше 10 уе
        }
        else
            p[i]=0;

        ui->transfer_te->append("d[" +QString().setNum(i) + "]: " + QString().setNum(d[i])+", "
                                +"p[" +QString().setNum(i) + "]: " + QString().setNum(p[i]) );
    }

    bool allTransfered=false;
    double dif=numeric_limits<double>::max();
    int donor_id=-1;
    int acceptor_id=-1;
    double toTransfer=0;
    int g=0;
    double max_dificit;

    while(!allTransfered)
    {
        allTransfered = true;

        //find first camp with deficit
        acceptor_id=-1;
        max_dificit=0;
        for(i=0;i<N;i++){
            if(d[i]==0) continue;

            if((total_t - t[i])>max_dificit)//поиск элемента с наименьшим сроком жизни
            {
                max_dificit = total_t - t[i];
                acceptor_id = i;
            }
        }
        //    ui->transfer_te->append("acceptor_id: " + QString().setNum(acceptor_id));
        if(acceptor_id==-1) break;

        //find camp with closest transfer money avilable
        dif=numeric_limits<double>::max();
        donor_id=-1;
        for(i=0;i<N;i++){
            if(p[i]<10)
            {
                p[i]=0;
                continue;
            }
            else
                allTransfered = false;

            if(abs(d[acceptor_id]-p[i])<dif){
                dif=abs(d[acceptor_id]-p[i]);
                donor_id = i;
            }//if
        }//for i

        if(donor_id==-1) break;

        ui->transfer_te->append("donor_id: " + QString().setNum(donor_id) + ", " + "acceptor_id: " + QString().setNum(acceptor_id));

        if( (donor_id>=0) && (acceptor_id>=0) && (!allTransfered) ){
            if(d[acceptor_id]>p[donor_id])
                toTransfer = p[donor_id];
            else
                toTransfer = d[acceptor_id];

            DAKList.append(new DAK(campStatList.at(donor_id)->id,campStatList.at(acceptor_id)->id,toTransfer));
            p[donor_id]-=toTransfer;
            d[acceptor_id]-=toTransfer;
            if(d[acceptor_id]<10) d[acceptor_id]=0;
            if(p[donor_id]<10) p[donor_id]=0;
            o[acceptor_id]+=toTransfer;
            o[donor_id]-=toTransfer;
        }//if

        if(g>20) break;

        g++;
    }//while

    // ui->txt->append("g = " + QString().setNum(g));

    return DAKList;

}

/////////////////////////////////////////////////////////////////////////////////////////////////

void provisor::switshTransfersVisible_slot()
{
    ui->transfer_te->setHidden(!ui->transfer_te->isHidden());
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::transfer(QString fromCampId, QString toCampId, double sum)
{

    QVariantList FromCampaigns, ToCampaigns;
    QVariantMap PayCampElementFrom, PayCampElementTo;
    Serializer serializer;
    QByteArray jsonRequest;
    QVariantMap transfers;
    QVariantMap makeTransferRequest;

    PayCampElementFrom.insert("CampaignID",fromCampId);
    PayCampElementFrom.insert("Sum",QString().setNum(sum,'f',2));

    PayCampElementTo.insert("CampaignID",toCampId);
    PayCampElementTo.insert("Sum",QString().setNum(sum,'f',2));

    FromCampaigns.append(PayCampElementFrom);
    ToCampaigns.append(PayCampElementTo);

    transfers.insert("FromCampaigns",FromCampaigns);
    transfers.insert("ToCampaigns",ToCampaigns);

    uint finOpNum = getNextFinOperationNum(currentFinOpNum);

    makeTransferRequest = fillRequestAuthData();
    makeTransferRequest.insert("param", transfers);
    makeTransferRequest.insert("method", "TransferMoney");
    makeTransferRequest.insert("operation_num", finOpNum);
    makeTransferRequest.insert("finance_token", getFinToken("TransferMoney",finOpNum));

    jsonRequest = serializer.serialize(makeTransferRequest);

    ui->transfer_te->append(QString(to_perfect_json(jsonRequest)).toStdString().data());

    /*
    networkReply = netManager.post(request,jsonRequest);
    connect(networkReply, SIGNAL(readyRead()), this, SLOT(slotReadyReadFromReply()));

    QTimer::singleShot(15000, this, SLOT(stopLoop()));
    loop.exec();

    //---------------------------------------------------------------

    json = networkReply->readAll();

*/


}

/////////////////////////////////////////////////////////////////////////////////////////////////
uint provisor::getNextFinOperationNum(uint currentNum)
{
    QSettings settings(SETTINGS_FILE_NAME,QSettings::IniFormat);
    settings.setIniCodec(QTextCodec::codecForName("utf8"));
    settings.sync();
    settings.beginGroup("settings");
    currentFinOpNum = currentNum+1;
    settings.setValue("currentFinOpNum",QString().setNum(currentNum+1));
    settings.sync();

    return currentNum+1;
}
/////////////////////////////////////////////////////////////////////////////////////////////////
QString provisor::getFinToken(QString method,uint finOpNum)
{

    QByteArray fillerString;
    fillerString.append(MasterToken);
    fillerString.append(QString().setNum(finOpNum));
    fillerString.append(method);
    fillerString.append(ui->login_te->text());

    cout << fillerString.data() << endl;

    string Message = fillerString.data();
    cout << Message << endl;

    string Calculated_Digest = STD_SHA_256::Digest (Message);

    cout << Calculated_Digest << endl;

    return QString(Calculated_Digest.data());
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::test_slot()
{
    transfer("123456","654321",200.1234);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
//{
//   "method": "TransferMoney",
//   "finance_token": (string),
//   "operation_num": (int),
//   "param": {
//      /* TransferMoneyInfo */
//      "FromCampaigns": [
//         {  /* PayCampElement */
//            "CampaignID": (int),
//            "Sum": (float)
//         }
//         ...
//      ],
//      "ToCampaigns": [
//         {  /* PayCampElement */
//            "CampaignID": (int),
//            "Sum": (float)
//         }
//         ...
//      ]
//   }
//}

/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::loadSettings()
{
    QSettings settings(SETTINGS_FILE_NAME,QSettings::IniFormat);
    settings.setIniCodec(QTextCodec::codecForName("utf8"));
    settings.sync();

    settings.beginGroup("settings");

    currentFinOpNum = settings.value("currentFinOpNum","0").toInt();
    groupSetPath = settings.value("groupSetPath","groupset.ini").toString();
    teleDataPath = settings.value("teleDataPath","").toString();

}

/////////////////////////////////////////////////////////////////////////////////////////////////

void provisor::commentChanged_slot(QString t)
{

    QSettings gSet(groupSetPath,QSettings::IniFormat);
    gSet.setIniCodec(QTextCodec::codecForName("utf8"));
    gSet.sync();

    //    QString groupName = ui->rests_tw->currentItem()->text(REST_TABLE_GROUP_NAME_COLUMN);
    //   QStringList gSetStr = gSet.childGroups();
    //    QString text = ((QTextEdit *)(ui->rests_tw->itemWidget(ui->rests_tw->currentItem(),REST_TABLE_COMMENT_COLUMN)))->toPlainText();
    QString comment = ((QLineEdit *)(ui->rests_tw->focusWidget()))->text();
    QString groupId = ((QLineEdit *)(ui->rests_tw->focusWidget()))->objectName();

    gSet.beginGroup(groupId);
    gSet.setValue("comment",comment);
    gSet.sync();
    /*
    foreach(QString g, gSetStr)
    {
        gSet.beginGroup(g);

        if(gSet.value("name").toString().compare(groupName)==0)
        {
            gSet.setValue("comment",comment);
            gSet.sync();
            return;
        }
        gSet.endGroup();
    }
            */
}
/////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::refreshStatData()
{
    QFile::remove((QString("%1stat%2.json").arg(teleDataPath).arg(DATE_STAMP)));

    loadTeleData();

    ui->statusBar->showMessage(QString(tr("Статистика загружена. Всего %1 элементов.").arg(QString().setNum(TodayStatParsedData.count()))),10000);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void provisor::refreshCampsData()
{
    QFile::remove((QString("%1camps%2.json").arg(teleDataPath).arg(DATE_STAMP)));
    //    QFile::remove((QString("%1clients%2.json").arg(teleDataPath).arg(DATE_STAMP)));

    loadTeleData();
    ui->statusBar->showMessage(QString(tr("Кампании загружены. Всего %1 элементов.").arg(QString().setNum(TodayCampsParsedData.count()))),10000);
}
/////////////////////////////////////////////////////////////////////////////////////////////////
