#include <QtDebug>

#include <QTableWidgetSelectionRange>
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QTimer>
#include <QList>

#include <bts_api.h>
#include <bts_client.h>

#include "sharedfolderswidget.h"
#include "addfolderdialog.h"
#include "folderinfodialog.h"
#include "utils.h"


SharedFoldersWidget::SharedFoldersWidget(QWidget *parent)
	:QWidget(parent)
	,client(nullptr)
	,api(nullptr)
{
	setupUi(this);

	foldersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	QTimer *updateTimer = new QTimer(this);
	updateTimer->setInterval(2000);
	updateTimer->start();

	connect(updateTimer, SIGNAL(timeout()), this, SLOT(updateTick()));

	connect(addButton, SIGNAL(clicked()), this, SLOT(addFolder()));
	connect(removeButton, SIGNAL(clicked()), this, SLOT(removeFolder()));
	connect(infoButton, SIGNAL(clicked()), this, SLOT(folderInfo()));

	connect(foldersTable, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(itemDoubleClicked(QTableWidgetItem*)));
}

void SharedFoldersWidget::setClient(BtsClient *newclient)
{
	if(client)
	{
		api->disconnect(this);
		api->deleteLater();
		api = nullptr;
	}

	client = newclient;

	api = new BtsApi(client, this);

	connect(api, SIGNAL(error(QString)), this, SIGNAL(error(QString)));

	connect(api, SIGNAL(getFoldersResult(QVector<BtsGetFoldersResult>,QString)),
	        this, SLOT(updateFolders(QVector<BtsGetFoldersResult>)));

	connect(client, SIGNAL(clientStarted()), this, SLOT(updateTick()));
	connect(api, SIGNAL(addFolderResult()), this, SLOT(updateTick()));
	connect(api, SIGNAL(removeFolderResult(QString)), this, SLOT(updateTick()));
}

void SharedFoldersWidget::addFolder()
{
	if(!api || !client)
		return;

	AddFolderDialog dialog(api, this);

	if(dialog.exec() != QDialog::Accepted)
		return;

	if(dialog.getPath().isEmpty())
		return;

	if(dialog.getSecret().isEmpty())
		return;

	api->addFolder(dialog.getPath(), dialog.getSecret());
}

static QTableWidgetItem *getSelectedRow(QTableWidget *tbl)
{
	QList<QTableWidgetSelectionRange> selections = tbl->selectedRanges();

	if(selections.size() != 1)
		return 0;

	QTableWidgetSelectionRange selection = selections.first();

	if(selection.rowCount() != 1)
		return 0;

	int row = selection.topRow();

	return tbl->item(row, 0);
}

void SharedFoldersWidget::removeFolder()
{
	if(!api || !client)
		return;

	QTableWidgetItem *item = getSelectedRow(foldersTable);

	if(!item)
		return;

	QString selectedFolder = item->text();
	QString selectedSecret = item->data(Qt::UserRole).toString();

	auto result = QMessageBox::question(this,
			tr("Remove shared folder?"),
			tr("Are you sure that you want to remove the shared folder \"%1\"?").arg(selectedFolder));

	if(result != QMessageBox::Yes)
		return;

	api->removeFolder(selectedSecret);
}

void SharedFoldersWidget::folderInfo()
{
	if(!api || !client || !client->isClientReady())
		return;

	QTableWidgetItem *item = getSelectedRow(foldersTable);

	if(!item)
		return;

	QString selectedSecret = item->data(Qt::UserRole).toString();

	if(selectedSecret.isEmpty())
	{
		qCritical("No secret associated with row!");
		return;
	}

	showInfoDialog(selectedSecret);
}

void SharedFoldersWidget::updateTick()
{
	if(!api || !client || !client->isClientReady())
		return;

	api->getFolders();
}

void SharedFoldersWidget::updateFolders(const QVector<BtsGetFoldersResult> result)
{
	if(foldersTable->hasFocus())
		return;

	if(foldersTable->rowCount() != result.size())
		foldersTable->setRowCount(result.size());

	int row = -1;
	for(const BtsGetFoldersResult &folder: result)
	{
		row += 1;

		QTableWidgetItem *pathItem = foldersTable->item(row, 0);
		QTableWidgetItem *sizeItem = foldersTable->item(row, 1);

		if(!pathItem)
		{
			pathItem = new QTableWidgetItem();
			foldersTable->setItem(row, 0, pathItem);
		}

		if(!sizeItem)
		{
			sizeItem = new QTableWidgetItem();
			foldersTable->setItem(row, 1, sizeItem);
		}

		pathItem->setData(Qt::UserRole, folder.secret);
		sizeItem->setData(Qt::UserRole, folder.secret);

		QString dir = folder.dir;

		if(dir.startsWith("\\\\?\\"))
			dir = dir.mid(4);

		QString sizeString = byteCountToString(folder.size, true, false);

		QString infoString = "";

		if(folder.indexing != 0)
			infoString += tr(" (indexing)");

		if(folder.error != 0)
			infoString += tr(" (error)");

		pathItem->setText(dir);
		sizeItem->setText(
			tr("%1 in %2%3")
				.arg(sizeString)
				.arg(tr("%Ln file(s)", "", folder.files))
				.arg(infoString)
		);
	}
}

void SharedFoldersWidget::itemDoubleClicked(QTableWidgetItem *item)
{
	QString selectedSecret = item->data(Qt::UserRole).toString();

	if(selectedSecret.isEmpty())
	{
		qCritical("No secret associated with row!");
		return;
	}

	showInfoDialog(selectedSecret);
}

void SharedFoldersWidget::showInfoDialog(const QString &secret)
{
	FolderInfoDialog *dialog = new FolderInfoDialog(api, secret, this);
	dialog->setModal(true);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->show();
}
