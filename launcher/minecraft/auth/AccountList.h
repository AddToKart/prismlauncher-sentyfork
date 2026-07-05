#pragma once

#include "MinecraftAccount.h"

#include <QAbstractListModel>
#include <QObject>
#include <QSharedPointer>
#include <QVariant>

class AccountList : public QAbstractListModel {
    Q_OBJECT
   public:
    enum ModelRoles { PointerRole = 0x34B1CB48 };

    enum VListColumns {
        ProfileNameColumn = 0,
        TypeColumn,
        StatusColumn,

        NUM_COLUMNS
    };

    explicit AccountList(QObject* parent = 0);
    virtual ~AccountList() noexcept;

    const MinecraftAccountPtr at(int i) const;
    int count() const;

    QVariant data(const QModelIndex& index, int role) const override;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    virtual int rowCount(const QModelIndex& parent) const override;
    virtual int columnCount(const QModelIndex& parent) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    void addAccount(MinecraftAccountPtr account);
    void removeAccount(QModelIndex index);
    void moveAccount(QModelIndex index, int delta);
    int findAccountByProfileId(const QString& profileId) const;
    MinecraftAccountPtr getAccountByProfileName(const QString& profileName) const;
    QStringList profileNames() const;

    void setListFilePath(QString path, bool autosave = false);

    bool loadList();
    bool loadV3(QJsonObject& root);
    bool saveList();

    MinecraftAccountPtr defaultAccount() const;
    void setDefaultAccount(MinecraftAccountPtr profileId);

    bool isActive() const;

   protected:
    void beginActivity();
    void endActivity();

   private:
    uint32_t m_activityCount = 0;
   signals:
    void listChanged();
    void listActivityChanged();
    void defaultAccountChanged();
    void activityChanged(bool active);

   public slots:
    void accountChanged();
    void accountActivityChanged(bool active);

   protected:
    void onListChanged();
    void onDefaultAccountChanged();

    QList<MinecraftAccountPtr> m_accounts;

    MinecraftAccountPtr m_defaultAccount;

    QString m_listFilePath;

    bool m_autosave = false;
};
