#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QPixmap>
#include <QString>

#include "AccountData.h"
#include "AuthSession.h"
#include "QObjectPtr.h"
#include "Usable.h"

class Task;
class MinecraftAccount;

using MinecraftAccountPtr = shared_qobject_ptr<MinecraftAccount>;
Q_DECLARE_METATYPE(MinecraftAccountPtr)

struct AccountProfile {
    QString id;
    QString name;
    bool legacy;
};

class MinecraftAccount : public QObject, public Usable {
    Q_OBJECT
   public:
    explicit MinecraftAccount(const MinecraftAccount& other, QObject* parent) = delete;

    explicit MinecraftAccount(QObject* parent = 0);

    static MinecraftAccountPtr createOffline(const QString& username);

    static MinecraftAccountPtr loadFromJsonV3(const QJsonObject& json);

    static QUuid uuidFromUsername(QString username);

    QJsonObject saveToJson() const;

   public:
    shared_qobject_ptr<Task> login();

   public:
    QString internalId() const { return data.internalId; }

    QString accessToken() const { return data.accessToken(); }

    QString profileId() const { return data.profileId(); }

    QString profileName() const { return data.profileName(); }

    QString displayName() const;

    bool isActive() const;

    AccountType accountType() const noexcept { return data.type; }

    bool hasProfile() const { return data.profileId().size() != 0; }

    QString typeString() const
    {
        return "offline";
    }

    QPixmap getFace(int width = 64, int height = 64) const;

    AccountState accountState() const;

    AccountData* accountData() { return &data; }

    bool shouldRefresh() const;

    void fillSession(AuthSessionPtr session);

    QString lastError() const { return data.lastError(); }

   signals:
    void changed();

    void activityChanged(bool active);

   protected:
    AccountData data;

    shared_qobject_ptr<Task> m_currentTask;

   protected:
    void incrementUses() override;
    void decrementUses() override;

   private slots:
    void authSucceeded();
    void authFailed(QString reason);
};
