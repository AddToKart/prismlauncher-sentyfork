#include "MinecraftAccount.h"

#include <QColor>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QUuid>

#include <QDebug>

#include <QPainter>

#include "minecraft/auth/AccountData.h"
#include "tasks/Task.h"

MinecraftAccount::MinecraftAccount(QObject* parent) : QObject(parent)
{
    data.internalId = QUuid::createUuid().toString(QUuid::Id128);
}

MinecraftAccountPtr MinecraftAccount::loadFromJsonV3(const QJsonObject& json)
{
    MinecraftAccountPtr account(new MinecraftAccount());
    if (account->data.resumeStateFromV3(json)) {
        return account;
    }
    return nullptr;
}

MinecraftAccountPtr MinecraftAccount::createOffline(const QString& username)
{
    auto account = makeShared<MinecraftAccount>();
    account->data.type = AccountType::Offline;
    account->data.yggdrasilToken.token = "0";
    account->data.yggdrasilToken.validity = Validity::Certain;
    account->data.yggdrasilToken.issueInstant = QDateTime::currentDateTimeUtc();
    account->data.yggdrasilToken.extra["userName"] = username;
    account->data.yggdrasilToken.extra["clientToken"] = QUuid::createUuid().toString(QUuid::Id128);
    account->data.minecraftProfile.id = uuidFromUsername(username).toString(QUuid::Id128);
    account->data.minecraftProfile.name = username;
    account->data.minecraftProfile.validity = Validity::Certain;
    return account;
}

QJsonObject MinecraftAccount::saveToJson() const
{
    return data.saveState();
}

AccountState MinecraftAccount::accountState() const
{
    return data.accountState;
}

QPixmap MinecraftAccount::getFace(int width, int height) const
{
    QPixmap skinTexture;
    if (!skinTexture.loadFromData(data.minecraftProfile.skin.data, "PNG")) {
        return QPixmap();
    }
    QPixmap skin = QPixmap(8, 8);
    skin.fill(QColorConstants::Transparent);
    QPainter painter(&skin);
    painter.drawPixmap(0, 0, skinTexture.copy(8, 8, 8, 8));
    painter.drawPixmap(0, 0, skinTexture.copy(40, 8, 8, 8));
    return skin.scaled(width, height, Qt::KeepAspectRatio);
}

shared_qobject_ptr<Task> MinecraftAccount::login()
{
    Q_ASSERT(m_currentTask.get() == nullptr);

    // Offline accounts don't need authentication
    m_currentTask.reset(nullptr);
    return m_currentTask;
}

void MinecraftAccount::authSucceeded()
{
    m_currentTask.reset();
    emit changed();
    emit activityChanged(false);
}

void MinecraftAccount::authFailed(QString reason)
{
    m_currentTask.reset();
    emit activityChanged(false);
}

QString MinecraftAccount::displayName() const
{
    if (const QList validStates{ AccountState::Unchecked, AccountState::Working, AccountState::Offline, AccountState::Online }; !validStates.contains(accountState())) {
        return QString("⚠ %1").arg(profileName());
    }
    return profileName();
}

bool MinecraftAccount::isActive() const
{
    return !m_currentTask.isNull();
}

bool MinecraftAccount::shouldRefresh() const
{
    return false;
}

void MinecraftAccount::fillSession(AuthSessionPtr session)
{
    session->access_token = data.accessToken();
    session->player_name = data.profileName();
    session->uuid = data.profileId();
    if (session->uuid.isEmpty())
        session->uuid = uuidFromUsername(session->player_name).toString(QUuid::Id128);
    session->user_type = typeString();
    if (!session->access_token.isEmpty()) {
        session->session = "token:" + data.accessToken() + ":" + data.profileId();
    } else {
        session->session = "-";
    }
}

void MinecraftAccount::decrementUses()
{
    Usable::decrementUses();
    if (!isInUse()) {
        emit changed();
        qWarning() << "Profile" << data.profileId() << "is no longer in use.";
    }
}

void MinecraftAccount::incrementUses()
{
    bool wasInUse = isInUse();
    Usable::incrementUses();
    if (!wasInUse) {
        emit changed();
        qWarning() << "Profile" << data.profileId() << "is now in use.";
    }
}

QUuid MinecraftAccount::uuidFromUsername(QString username)
{
    auto input = QString("OfflinePlayer:%1").arg(username).toUtf8();

    QByteArray digest = QCryptographicHash::hash(input, QCryptographicHash::Md5);

    auto bOr = [](QByteArray& array, qsizetype index, uint8_t value) { array[index] |= value; };
    auto bAnd = [](QByteArray& array, qsizetype index, uint8_t value) { array[index] &= value; };
    bAnd(digest, 6, 0x0f);
    bOr(digest, 6, 0x30);
    bAnd(digest, 8, 0x3f);
    bOr(digest, 8, 0x80);

    return QUuid::fromRfc4122(digest);
}
