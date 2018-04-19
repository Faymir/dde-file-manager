

#include <thread>
#include <chrono>
#include <random>


#include "singleton.h"
#include "tagmanager.h"
#include "tag/tagutil.h"
#include "shutil/dsqlitehandle.h"
#include "app/filesignalmanager.h"
#include "controllers/appcontroller.h"
#include "controllers/tagmanagerdaemoncontroller.h"

#include <QMap>
#include <QDebug>
#include <QVariant>



std::once_flag TagManager::onceFlag{};


std::multimap<TagManager::SqlType, QString> SqlStr{
                                                        { TagManager::SqlType::GetAllTags, "SELECT * FROM tag_property" },
                                                        { TagManager::SqlType::GetFilesThroughTag, "SELECT COUNT(tag_property.tag_name) AS counting FROM tag_property "
                                                                                                   "WHERE tag_property.tag_name = \'%1\'" },
                                                        { TagManager::SqlType::MakeFilesTags, "SELECT COUNT(tag_property.tag_name) AS counting FROM tag_property "
                                                                                                                                               "WHERE tag_property.tag_name = \'%1\'"},
                                                        { TagManager::SqlType::MakeFilesTags, "INSERT INTO tag_property (tag_name, tag_color)  "
                                                                                                                "VALUES (\'%1\', \'%2\')" },
                                                        { TagManager::SqlType::MakeFilesTags, "SELECT tag_property.tag_name FROM tag_property" },

                                                        { TagManager::SqlType::MakeFilesTagThroughColor, "SELECT COUNT(tag_property.tag_name) AS counting FROM tag_property "
                                                                                                                                    "WHERE tag_property.tag_name = \'%1\'" },
                                                        { TagManager::SqlType::MakeFilesTagThroughColor, "INSERT INTO tag_property (tag_name, tag_color) VALUES (\'%1\', \'%2\')" }
                                                  };




static QString randomColor() noexcept
{
    std::random_device device{};

    // Choose a random mean between 0 and 6
    std::default_random_engine engine(device());
    std::uniform_int_distribution<int> uniform_dist(0, 6);
    return  Tag::ColorName[uniform_dist(engine)];
}


QMap<QString, QString> TagManager::getAllTags()
{
    impl::shared_mutex<QReadWriteLock> sharedLck{ mutex, impl::shared_mutex<QReadWriteLock>::Options::Read };
    ++m_counter;
    QMap<QString, QString> tagNameAndColor{};

    if(sqlDataBase.open()){
        QSqlQuery sqlQuery{ sqlDataBase };
        std::pair<std::multimap<TagManager::SqlType, QString>::const_iterator,
                  std::multimap<TagManager::SqlType, QString>::const_iterator> range{ SqlStr.equal_range(TagManager::SqlType::GetAllTags) };

        if(range.first != SqlStr.cend()){

            if(sqlQuery.exec(range.first->second)){
                qWarning(sqlQuery.lastError().text().toStdString().c_str());
            }

            while(sqlQuery.next()){
                QString tagName{ sqlQuery.value("tag_name").toString() };
                QString tagColor{ sqlQuery.value("tag_color").toString() };
                tagNameAndColor[tagName] = tagColor;
            }
        }
    }


    --m_counter;
    this->closeSqlDatabase();

    return tagNameAndColor;
}

QList<QString> TagManager::getSameTagsOfDiffFiles(const QList<DUrl>& files)
{
    impl::shared_mutex<QReadWriteLock> sharedLck{ mutex, impl::shared_mutex<QReadWriteLock>::Options::Read};
    ++m_counter;
    QMap<QString, QVariant> varMap{};

    for(const DUrl& url : files){
        varMap[url.toLocalFile()] = QVariant{ QList<QString>{} };
    }

    QVariant var{ TagManagerDaemonController::instance()->disposeClientData(varMap,
                                              TagManager::getCurrentUserName(), Tag::ActionType::GetTagsThroughFile) };

    --m_counter;
    this->closeSqlDatabase();

    return var.toStringList();
}

QMap<QString, QColor> TagManager::getTagColor(const QList<QString>& tags)
{
    impl::shared_mutex<QReadWriteLock> sharedLck{ mutex, impl::shared_mutex<QReadWriteLock>::Options::Read};
    ++m_counter;
    QMap<QString, QColor> tagNameAndColor{};

    if(!tags.isEmpty()){
      QString sqlStr{ "SELECT * FROM tag_property WHERE tag_property.tag_name = \'%1\'" };

      if(sqlDataBase.open()){
          QSqlQuery sqlQuery{ sqlDataBase };

          for(const QString& tagName : tags){
              QString sql{ sqlStr };
              sql = sql.arg(tagName);

              if(!sqlQuery.exec(sql)){
                  qWarning(sqlQuery.lastError().text().toStdString().c_str());

              }else{

                  if(sqlQuery.next()){
                      QString tagColor{ sqlQuery.value("tag_color").toString() };

                      if(!tagColor.isEmpty()){
//                          qDebug()<< "color name: " << Tag::NamesWithColors[tagColor].name();
                          tagNameAndColor[tagName] = Tag::NamesWithColors[tagColor];

                      }else{
                          qWarning()<< "Can not get the color of specify tag: "<< tagName;
                      }
                  }
              }
          }
      }
    }


    --m_counter;
    this->closeSqlDatabase();

    return tagNameAndColor;
}

QList<QString> TagManager::getFilesThroughTag(const QString& tagName)
{
    impl::shared_mutex<QReadWriteLock> sharedLck{ mutex, impl::shared_mutex<QReadWriteLock>::Options::Read};
    ++m_counter;
    QList<QString> filesPath{};

    if(sqlDataBase.open()){
        QSqlQuery sqlQuery{ sqlDataBase };
        std::pair<std::multimap<TagManager::SqlType, QString>::const_iterator,
                  std::multimap<TagManager::SqlType, QString>::const_iterator> range{ SqlStr.equal_range(TagManager::SqlType::GetFilesThroughTag) };

        if(range.first != SqlStr.cend()){
            QString sqlForQueryTag{ range.first->second.arg(tagName) };

            if(sqlQuery.exec(sqlForQueryTag)){
                qWarning(sqlQuery.lastError().text().toStdString().c_str());
            }

            if(sqlQuery.next()){
                int amount{ sqlQuery.value(0).toInt() };

                if(amount > 0){
                    QMap<QString, QVariant> varMap{ {tagName, QVariant{QList<QString>{}}} };
                    QVariant var{ TagManagerDaemonController::instance()->disposeClientData(varMap,
                                                                                            TagManager::getCurrentUserName(), Tag::ActionType::GetFilesThroughTag) };
                    filesPath = var.toStringList();
                }
            }
        }
    }


    --m_counter;
    this->closeSqlDatabase();

    return filesPath;
}

bool TagManager::makeFilesTags(const QList<QString>& tags, const QList<DUrl>& files)
{

    impl::shared_mutex<QReadWriteLock> sharedLck{ mutex,impl::shared_mutex<QReadWriteLock>::Options::Write};
    ++m_counter;
    bool resultValue{ false };

    if(sqlDataBase.open() && sqlDataBase.transaction()){
        QSqlQuery sqlQuery{ sqlDataBase };
        std::pair<std::multimap<TagManager::SqlType, QString>::const_iterator,
                  std::multimap<TagManager::SqlType, QString>::const_iterator> range{ SqlStr.equal_range(TagManager::SqlType::MakeFilesTags) };

        if(range.first != SqlStr.cend()){
            bool flag{ false };

            for(const QString& tagName : tags){
                QString sqlForCountingTag{ range.first->second.arg(tagName) };

                if(!sqlQuery.exec(sqlForCountingTag)){
                    flag = true;
                    qWarning(sqlQuery.lastError().text().toStdString().c_str());
                }

                if(sqlQuery.next()){
                    int amount{ sqlQuery.value("counting").toInt() };

                    if(amount > 0){
                        continue;

                    }else{
                        std::multimap<TagManager::SqlType, QString>::const_iterator itrForInsertTag{ range.first };
                        ++itrForInsertTag;

                        std::vector<QString>::const_iterator cbeg{ Tag::ColorName.cbegin() };
                        std::vector<QString>::const_iterator cend{ Tag::ColorName.cend() };
                        std::vector<QString>::const_iterator pos{ std::find(cbeg, cend, tagName) };
                        QString sqlForInsertingTag{ itrForInsertTag->second.arg(tagName) };

                        if(pos == cend){
                            sqlForInsertingTag = sqlForInsertingTag.arg(randomColor());

                        }else{
                            sqlForInsertingTag = sqlForInsertingTag.arg(*pos);
                        }

                        if(!sqlQuery.exec(sqlForInsertingTag)){
                            flag = true;
                            qWarning(sqlQuery.lastError().text().toStdString().c_str());
                        }
                    }
                }
            }

            if(!flag && sqlDataBase.commit()){
                QMap<QString, QVariant> filesAndTags{};

                for(const DUrl& url : files){
                    filesAndTags[url.toLocalFile()] = QVariant{tags};
                }

                QVariant var{ TagManagerDaemonController::instance()->disposeClientData(filesAndTags,
                                                                                TagManager::getCurrentUserName(), Tag::ActionType::MakeFilesTags) };
                resultValue = var.toBool();
            }else{
                sqlDataBase.rollback();
            }
        }
    }

    --m_counter;
    this->closeSqlDatabase();

    if (resultValue) {
        emit fileSignalManager->fileTagInfoChanged(files);
    }

    return resultValue;
}

bool TagManager::changeTagColor(const QString& tagName, const QPair<QString, QString>& oldAndNewTagColor)
{
    impl::shared_mutex<QReadWriteLock> sharedLck{ mutex, impl::shared_mutex<QReadWriteLock>::Options::Write};
    ++m_counter;

    if(!tagName.isEmpty() && !oldAndNewTagColor.first.isEmpty() && !oldAndNewTagColor.second.isEmpty()){

        if(sqlDataBase.open() && sqlDataBase.transaction()){
            QSqlQuery sqlQuery{ sqlDataBase };
            QString sqlStr{ "UPDATE tag_property SET tag_color = \'%1\' WHERE tag_property.tag_color = \'%2\' "
                            "AND tag_property.tag_name = \'%3\'" };
            sqlStr = sqlStr.arg(oldAndNewTagColor.second);
            sqlStr = sqlStr.arg(oldAndNewTagColor.first);
            sqlStr = sqlStr.arg(tagName);

            if(!sqlQuery.exec(sqlStr)){
                qWarning(sqlQuery.lastError().text().toStdString().c_str());
            }

            if(!sqlDataBase.commit()){
                sqlDataBase.rollback();
                --m_counter;
                this->closeSqlDatabase();

                return false;
            }

            --m_counter;
            this->closeSqlDatabase();

            return true;
        }
    }

    --m_counter;
    this->closeSqlDatabase();

    return false;
}

bool TagManager::remveTagsOfFiles(const QList<QString>& tags, const QList<DUrl>& files)
{
    impl::shared_mutex<QReadWriteLock> sharedLck{ mutex, impl::shared_mutex<QReadWriteLock>::Options::Write };
    ++m_counter;
    bool value{ false };

    if(!tags.isEmpty() && !files.isEmpty()){
        QMap<QString, QVariant> filesAndTags{};

        for(const DUrl& url : files){
            filesAndTags[url.toLocalFile()] = QVariant{tags};
        }

        QVariant var{ TagManagerDaemonController::instance()->disposeClientData(filesAndTags,
                                                                                TagManager::getCurrentUserName(), Tag::ActionType::RemoveTagsOfFiles) };

        value = var.toBool();
    }

    --m_counter;
    this->closeSqlDatabase();

    if (value) {
        emit fileSignalManager->fileTagInfoChanged(files);
    }

    return value;
}

bool TagManager::deleteTags(const QList<QString>& tags)
{
    DUrlList filesOfTags;

    for (const QString &tag : tags) {
        for (const QString &path : getFilesThroughTag(tag)) {
            filesOfTags << DUrl::fromLocalFile(path);
        }
    }

    impl::shared_mutex<QReadWriteLock> sharedLck{ mutex, impl::shared_mutex<QReadWriteLock>::Options::Write };
    ++m_counter;

    if(!tags.isEmpty()){

        if(sqlDataBase.open() && sqlDataBase.transaction()){
            bool value{ false };
            QString sqlStr{ "DELETE FROM tag_property WHERE tag_property.tag_name = \'%1\'" };

            for(const QString& tag : tags){
                QSqlQuery sqlQuery{ sqlDataBase };
                sqlStr = sqlStr.arg(tag);

                if(!sqlQuery.exec(sqlStr)){
                    value = true;
                    qWarning(sqlQuery.lastError().text().toStdString().c_str());
                }

                sqlQuery.clear();
            }

            if(!(!value && sqlDataBase.commit())){
                sqlDataBase.rollback();
                --m_counter;
                this->closeSqlDatabase();

                return false;
            }
        }

        QMap<QString, QVariant> tagsAsKeys{};

        for(const QString& tag : tags){
            tagsAsKeys[tag] = QVariant{ QList<QString>{} };
        }

        QVariant var{ TagManagerDaemonController::instance()->disposeClientData(tagsAsKeys,
                                                  TagManager::getCurrentUserName(), Tag::ActionType::DeleteTags) };

        --m_counter;
        this->closeSqlDatabase();

        if (var.toBool() && !filesOfTags.isEmpty()) {
            emit fileSignalManager->fileTagInfoChanged(filesOfTags);
        }

        return var.toBool();
    }

    --m_counter;
    this->closeSqlDatabase();

    return false;
}

bool TagManager::deleteFiles(const QList<DUrl>& fileList)
{
    if(!fileList.isEmpty()){
        QList<QString> urlList{};

        for(const DUrl& localFile : fileList){
            QString localFileStr{ localFile.toLocalFile() };
            urlList.push_back(localFileStr);
        }

        if(!urlList.isEmpty()){
            bool value{ TagManager::instance()->deleteFiles(urlList) };

            return value;
        }
    }

    return false;
}

bool TagManager::deleteFiles(const QList<QString>& fileList)
{
    if(!fileList.isEmpty()){
        QMap<QString, QVariant> filesForDeleting{};

        for(const QString& file : fileList){
            filesForDeleting[file] = QVariant{ QList<QString>{} };
        }

        if(!filesForDeleting.isEmpty()){
            QVariant var{ TagManagerDaemonController::instance()->disposeClientData(filesForDeleting,
                                                      TagManager::getCurrentUserName(), Tag::ActionType::DeleteFiles) };
            return var.toBool();
        }
    }

    return false;
}

bool TagManager::changeTagName(const QPair<QString, QString>& oldAndNewName)
{
    impl::shared_mutex<QReadWriteLock> sharedLck{ mutex, impl::shared_mutex<QReadWriteLock>::Options::Write };
    ++m_counter;

    if(!oldAndNewName.first.isEmpty() && !oldAndNewName.second.isEmpty()){
        bool flagForUpdatingMainDB{ false };
        QString sqlStr{ "UPDATE tag_property SET tag_name = \'%1\' WHERE tag_property.tag_name = \'%2\'" };

        if(sqlDataBase.open() && TagManager::sqlDataBase.transaction()){

            QSqlQuery sqlQuery{ sqlDataBase };
            sqlStr = sqlStr.arg(oldAndNewName.second);
            sqlStr = sqlStr.arg(oldAndNewName.first);

            if(!sqlQuery.exec(sqlStr)){
                flagForUpdatingMainDB = true;
                qWarning(sqlQuery.lastError().text().toStdString().c_str());
            }

            if(!(!flagForUpdatingMainDB && sqlDataBase.commit())){
                flagForUpdatingMainDB = true;
                sqlDataBase.rollback();
            }

            QVariant var{};
            QMap<QString, QVariant> oldAndNew{ {oldAndNewName.first, QVariant{ QList<QString>{ oldAndNewName.second } }} };

            if(!flagForUpdatingMainDB){
                var = TagManagerDaemonController::instance()->disposeClientData(oldAndNew,
                                                              TagManager::getCurrentUserName(), Tag::ActionType::ChangeTagName);
            }

            if(!flagForUpdatingMainDB && var.toBool()){
                --m_counter;
                this->closeSqlDatabase();

                return true;
            }
        }
    }

    --m_counter;
    this->closeSqlDatabase();

    return false;
}

bool TagManager::makeFilesTagThroughColor(const QString& color, const QList<DUrl>& files)
{
    impl::shared_mutex<QReadWriteLock> sharedLck{ mutex, impl::shared_mutex<QReadWriteLock>::Options::Write };
    ++m_counter;

    if(!color.isEmpty() && !files.isEmpty()){
        QString colorName{ Tag::ColorsWithNames[color] };

        if(!colorName.isEmpty()){
            std::pair<std::multimap<TagManager::SqlType, QString>::const_iterator,
                      std::multimap<TagManager::SqlType, QString>::const_iterator> range{ SqlStr.equal_range(TagManager::SqlType::MakeFilesTagThroughColor) };
            QString sqlForCounting{ range.first->second.arg(colorName) };

            if(sqlDataBase.open() && sqlDataBase.transaction()){
                QSqlQuery sqlQuery{ sqlDataBase };

                if(!sqlQuery.exec(sqlForCounting)){
                    qWarning(sqlQuery.lastError().text().toStdString().c_str());
                }

                if(sqlQuery.next()){
                    int counting{ sqlQuery.value("counting").toInt() };

                    if(counting == 0){
                        std::multimap<TagManager::SqlType, QString>::const_iterator sqlItrBeg{ range.first };
                        ++sqlItrBeg;

                        QString sqlForInserting{ sqlItrBeg->second.arg(colorName) };
                        sqlForInserting = sqlForInserting.arg(colorName);

                        sqlQuery.clear();

                        if(!sqlQuery.exec(sqlForInserting)){
                            qWarning(sqlQuery.lastError().text().toStdString().c_str());
                            sqlDataBase.rollback();
                            --m_counter;
                            this->closeSqlDatabase();

                            return false;
                        }
                    }

                    QMap<QString, QVariant> filesAndColorName{};
                    QVariant dbusResult{};

                    for(const DUrl& file : files){
                        filesAndColorName[file.toLocalFile()] = QVariant{ QList<QString>{colorName} };
                    }


                    dbusResult =  TagManagerDaemonController::instance()->disposeClientData(filesAndColorName, TagManager::getCurrentUserName(),
                                                                                            Tag::ActionType::MakeFilesTagThroughColor);
                    if(dbusResult.toBool() && sqlDataBase.commit()){
                        --m_counter;
                        this->closeSqlDatabase();

                        return true;
                    }

                    if(!dbusResult.toBool()){
                        sqlDataBase.rollback();
                        --m_counter;
                        this->closeSqlDatabase();

                        return false;
                    }
                }
            }
        }
    }

    --m_counter;
    this->closeSqlDatabase();

    return false;
}

bool TagManager::changeFilesName(const QList<QPair<DUrl, DUrl>>& oldAndNewFilesName)
{
    impl::shared_mutex<QReadWriteLock> sharedLck{ mutex, impl::shared_mutex<QReadWriteLock>::Options::Write };

    if(!oldAndNewFilesName.isEmpty()){
        QMap<QString, QVariant> oldNewFilesName{};

        for(const QPair<DUrl, DUrl>& oldAndNewFileName : oldAndNewFilesName){
            oldNewFilesName[oldAndNewFileName.first.toLocalFile()] = QVariant{ QList<QString>{ oldAndNewFileName.second.toLocalFile() } };
        }

        QVariant dbusResult{ TagManagerDaemonController::instance()->disposeClientData(oldNewFilesName, TagManager::getCurrentUserName(),
                                                                                       Tag::ActionType::ChangeFilesName) };
        return dbusResult.toBool();
    }

    return false;
}

QString TagManager::getMainDBLocation()
{
//    passwd* pwd = getpwuid(getuid());
    QString homePath{QDir::homePath()};

    qDebug()<< "homePath:  " << homePath;
    QString mainDBLocation{ homePath + QString{"/.config/deepin/dde-file-manager/"} + QString{"main.db" } };

    return mainDBLocation;
}