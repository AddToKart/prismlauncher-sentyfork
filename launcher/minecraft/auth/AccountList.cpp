#include "AccountList.h"
#include "AccountData.h"
#include "tasks/Task.h"

#include <QDir>
#include <QFile>
#include <QIcon>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTextStream>

#include <QDebug>

#include <FileSystem.h>
#include <QSaveFile>

enum AccountListVersion { Current = 3 };

AccountList::AccountList(QObject* parent) : QAbstractListModel(parent) {}

AccountList::~AccountList() noexcept {}

int AccountList::findAccountByProfileId(const QString& profileId) const
{
    for (int i = 0; i < count(); i++) {
        MinecraftAccountPtr account = at(i);
        if (account->profileId() == profileId) {
            return i;
        }
    }
    return -1;
}

MinecraftAccountPtr AccountList::getAccountByProfileName(const QString& profileName) const
{
    for (int i = 0; i < count(); i++) {
        MinecraftAccountPtr account = at(i);
        if (account->profileName() == profileName) {
            return account;
        }
    }
    return nullptr;
}

const MinecraftAccountPtr AccountList::at(int i) const
{
    return MinecraftAccountPtr(m_accounts.at(i));
}

QStringList AccountList::profileNames() const
{
    QStringList out;
    for (auto& account : m_accounts) {
        auto profileName = account->profileName();
        if (profileName.isEmpty()) {
            continue;
        }
        out.append(profileName);
    }
    return out;
}

void AccountList::addAccount(const MinecraftAccountPtr account)
{
    if (m_accounts.contains(account)) {
        qDebug() << "Tried to add account that's already on the accounts list!";
        return;
    }

    connect(account.get(), &MinecraftAccount::changed, this, &AccountList::accountChanged);
    connect(account.get(), &MinecraftAccount::activityChanged, this, &AccountList::accountActivityChanged);

    auto profileId = account->profileId();
    if (profileId.size()) {
        auto existingAccount = findAccountByProfileId(profileId);
        if (existingAccount != -1) {
            qDebug() << "Replacing old account with a new one with the same profile ID!";

            MinecraftAccountPtr existingAccountPtr = m_accounts[existingAccount];
            m_accounts[existingAccount] = account;
            if (m_defaultAccount == existingAccountPtr) {
                m_defaultAccount = account;
            }
            existingAccountPtr->disconnect(this);
            emit dataChanged(index(existingAccount), index(existingAccount, columnCount(QModelIndex()) - 1));
            onListChanged();
            return;
        }
    }

    int row = m_accounts.count();
    qDebug() << "Inserting account at index" << row;

    beginInsertRows(QModelIndex(), row, row);
    m_accounts.append(account);
    endInsertRows();

    onListChanged();
}

void AccountList::removeAccount(QModelIndex index)
{
    int row = index.row();
    if (index.isValid() && row >= 0 && row < m_accounts.size()) {
        auto& account = m_accounts[row];
        if (account == m_defaultAccount) {
            m_defaultAccount = nullptr;
            onDefaultAccountChanged();
        }
        account->disconnect(this);

        beginRemoveRows(QModelIndex(), row, row);
        m_accounts.removeAt(index.row());
        endRemoveRows();
        onListChanged();
    }
}

void AccountList::moveAccount(QModelIndex index, int delta)
{
    const int row = index.row();
    const int newRow = row + delta;
    if (index.isValid() && row < m_accounts.size() && newRow >= 0 && newRow < m_accounts.size()) {
        const int modelDestinationRow = (newRow > row) ? newRow + 1 : newRow;

        if (beginMoveRows(QModelIndex(), row, row, QModelIndex(), modelDestinationRow)) {
            m_accounts.move(row, newRow);
            endMoveRows();

            onListChanged();
        } else {
            qCritical().noquote() << "AccountList: failed to move account from" << row << "to" << newRow
                                  << QString("(%1 accounts in total)").arg(this->count());
        }
    }
}

MinecraftAccountPtr AccountList::defaultAccount() const
{
    return m_defaultAccount;
}

void AccountList::setDefaultAccount(MinecraftAccountPtr newAccount)
{
    if (!newAccount && m_defaultAccount) {
        int idx = 0;
        auto previousDefaultAccount = m_defaultAccount;
        m_defaultAccount = nullptr;
        for (MinecraftAccountPtr account : m_accounts) {
            if (account == previousDefaultAccount) {
                emit dataChanged(index(idx), index(idx, columnCount(QModelIndex()) - 1));
            }
            idx++;
        }
        onDefaultAccountChanged();
    } else {
        auto currentDefaultAccount = m_defaultAccount;
        int currentDefaultAccountIdx = -1;
        auto newDefaultAccount = m_defaultAccount;
        int newDefaultAccountIdx = -1;
        int idx = 0;
        for (MinecraftAccountPtr account : m_accounts) {
            if (account == newAccount) {
                newDefaultAccount = account;
                newDefaultAccountIdx = idx;
            }
            if (currentDefaultAccount == account) {
                currentDefaultAccountIdx = idx;
            }
            idx++;
        }
        if (currentDefaultAccount != newDefaultAccount) {
            emit dataChanged(index(currentDefaultAccountIdx), index(currentDefaultAccountIdx, columnCount(QModelIndex()) - 1));
            emit dataChanged(index(newDefaultAccountIdx), index(newDefaultAccountIdx, columnCount(QModelIndex()) - 1));
            m_defaultAccount = newDefaultAccount;
            onDefaultAccountChanged();
        }
    }
}

void AccountList::accountChanged()
{
    onListChanged();
}

void AccountList::accountActivityChanged(bool active)
{
    MinecraftAccount* account = qobject_cast<MinecraftAccount*>(sender());
    bool found = false;
    for (int i = 0; i < count(); i++) {
        if (at(i).get() == account) {
            emit dataChanged(index(i), index(i, columnCount(QModelIndex()) - 1));
            found = true;
            break;
        }
    }
    if (found) {
        emit listActivityChanged();
        if (active) {
            beginActivity();
        } else {
            endActivity();
        }
    }
}

void AccountList::onListChanged()
{
    if (m_autosave)
        saveList();

    emit listChanged();
}

void AccountList::onDefaultAccountChanged()
{
    if (m_autosave)
        saveList();

    emit defaultAccountChanged();
}

int AccountList::count() const
{
    return m_accounts.count();
}

QString getAccountStatus(AccountState status)
{
    switch (status) {
        case AccountState::Unchecked:
            return QObject::tr("Unchecked", "Account status");
        case AccountState::Offline:
            return QObject::tr("Offline", "Account status");
        case AccountState::Online:
            return QObject::tr("Ready", "Account status");
        case AccountState::Working:
            return QObject::tr("Working", "Account status");
        case AccountState::Errored:
            return QObject::tr("Errored", "Account status");
        case AccountState::Expired:
            return QObject::tr("Expired", "Account status");
        case AccountState::Disabled:
            return QObject::tr("Disabled", "Account status");
        case AccountState::Gone:
            return QObject::tr("Gone", "Account status");
        default:
            return QObject::tr("Unknown", "Account status");
    }
}

QVariant AccountList::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() > count())
        return QVariant();

    MinecraftAccountPtr account = at(index.row());

    switch (role) {
        case Qt::SizeHintRole:
            if (index.column() == ProfileNameColumn) {
                return QSize(0, 30);
            }

            return QVariant();
        case Qt::DecorationRole:
            if (index.column() == ProfileNameColumn) {
                auto face = account->getFace(24, 24);

                if (!face.isNull()) {
                    return face;
                } else {
                    return QIcon::fromTheme("noaccount").pixmap(24, 24);
                }
            }

            return QVariant();
        case Qt::DisplayRole:
            switch (index.column()) {
                case ProfileNameColumn:
                    return account->profileName();
                case TypeColumn: {
                    return tr("Offline", "Account type");
                }
                case StatusColumn:
                    return getAccountStatus(account->accountState());
                default:
                    return QVariant();
            }

        case PointerRole:
            return QVariant::fromValue(account);

        case Qt::CheckStateRole:
            if (index.column() == ProfileNameColumn)
                return account == m_defaultAccount ? Qt::Checked : Qt::Unchecked;
            return QVariant();

        default:
            return QVariant();
    }
}

QVariant AccountList::headerData(int section, [[maybe_unused]] Qt::Orientation orientation, int role) const
{
    switch (role) {
        case Qt::DisplayRole:
            switch (section) {
                case ProfileNameColumn:
                    return tr("Username");
                case TypeColumn:
                    return tr("Type");
                case StatusColumn:
                    return tr("Status");
                default:
                    return QVariant();
            }

        case Qt::ToolTipRole:
            switch (section) {
                case ProfileNameColumn:
                    return tr("Minecraft username associated with the account.");
                case TypeColumn:
                    return tr("Type of the account");
                case StatusColumn:
                    return tr("Current status of the account.");
                default:
                    return QVariant();
            }

        default:
            return QVariant();
    }
}

int AccountList::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : count();
}

int AccountList::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : NUM_COLUMNS;
}

Qt::ItemFlags AccountList::flags(const QModelIndex& index) const
{
    if (index.row() < 0 || index.row() >= rowCount(index.parent()) || !index.isValid()) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool AccountList::setData(const QModelIndex& idx, const QVariant& value, int role)
{
    if (idx.row() < 0 || idx.row() >= rowCount(idx.parent()) || !idx.isValid()) {
        return false;
    }

    if (role == Qt::CheckStateRole) {
        if (value == Qt::Checked) {
            MinecraftAccountPtr account = at(idx.row());
            setDefaultAccount(account);
        } else if (m_defaultAccount == at(idx.row()))
            setDefaultAccount(nullptr);
    }

    emit dataChanged(idx, index(idx.row(), columnCount(QModelIndex()) - 1));
    return true;
}

bool AccountList::loadList()
{
    if (m_listFilePath.isEmpty()) {
        qCritical() << "Can't load account list. No file path given and no default set.";
        return false;
    }

    QFile file(m_listFilePath);

    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << QString("Failed to read the account list file %1 (%2).").arg(m_listFilePath).arg(file.errorString()).toUtf8();
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qCritical() << QString("Failed to parse account list file: %1 at offset %2")
                           .arg(parseError.errorString(), QString::number(parseError.offset))
                           .toUtf8();
        return false;
    }

    if (!jsonDoc.isObject()) {
        qCritical() << "Invalid account list JSON: Root should be an array.";
        return false;
    }

    QJsonObject root = jsonDoc.object();

    auto listVersion = root.value("formatVersion").toVariant().toInt();
    if (listVersion == AccountListVersion::Current)
        return loadV3(root);

    QString newName = "accounts-old.json";
    qWarning() << "Unknown format version when loading account list. Existing one will be renamed to" << newName;
    file.rename(newName);
    return false;
}

bool AccountList::loadV3(QJsonObject& root)
{
    beginResetModel();
    QJsonArray accounts = root.value("accounts").toArray();
    for (QJsonValue accountVal : accounts) {
        QJsonObject accountObj = accountVal.toObject();
        MinecraftAccountPtr account = MinecraftAccount::loadFromJsonV3(accountObj);
        if (account.get() != nullptr) {
            auto profileId = account->profileId();
            if (profileId.size()) {
                if (findAccountByProfileId(profileId) != -1) {
                    continue;
                }
            }
            connect(account.get(), &MinecraftAccount::changed, this, &AccountList::accountChanged);
            connect(account.get(), &MinecraftAccount::activityChanged, this, &AccountList::accountActivityChanged);
            m_accounts.append(account);
            if (accountObj.value("active").toBool(false)) {
                m_defaultAccount = account;
            }
        } else {
            qWarning() << "Failed to load an account.";
        }
    }
    endResetModel();
    return true;
}

bool AccountList::saveList()
{
    if (m_listFilePath.isEmpty()) {
        qCritical() << "Can't save account list. No file path given and no default set.";
        return false;
    }

    if (!FS::ensureFilePathExists(m_listFilePath))
        return false;

    QFileInfo finfo(m_listFilePath);
    if (finfo.isDir()) {
        QDir badDir(m_listFilePath);
        badDir.removeRecursively();
    }

    qDebug() << "Writing account list to" << m_listFilePath;

    qDebug() << "Building JSON data structure.";
    QJsonObject root;

    root.insert("formatVersion", AccountListVersion::Current);

    qDebug() << "Building account array.";
    QJsonArray accounts;
    for (MinecraftAccountPtr account : m_accounts) {
        QJsonObject accountObj = account->saveToJson();
        if (m_defaultAccount == account) {
            accountObj["active"] = true;
        }
        accounts.append(accountObj);
    }

    root.insert("accounts", accounts);

    QJsonDocument doc(root);

    qDebug() << "Writing account list to file.";
    QSaveFile file(m_listFilePath);

    if (!file.open(QIODevice::WriteOnly)) {
        qCritical() << QString("Failed to save the account list file %1 (%2).").arg(m_listFilePath).arg(file.errorString()).toUtf8();
        return false;
    }

    file.write(doc.toJson());
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    if (file.commit()) {
        qDebug() << "Saved account list to" << m_listFilePath;
        return true;
    } else {
        qDebug() << "Failed to save accounts to" << m_listFilePath << "error:" << file.errorString();
        return false;
    }
}

void AccountList::setListFilePath(QString path, bool autosave)
{
    m_listFilePath = path;
    m_autosave = autosave;
}

bool AccountList::isActive() const
{
    return m_activityCount != 0;
}

void AccountList::beginActivity()
{
    bool activating = m_activityCount == 0;
    m_activityCount++;
    if (activating) {
        emit activityChanged(true);
    }
}

void AccountList::endActivity()
{
    if (m_activityCount == 0) {
        qWarning() << "Activity count would become below zero";
        return;
    }
    bool deactivating = m_activityCount == 1;
    m_activityCount--;
    if (deactivating) {
        emit activityChanged(false);
    }
}
