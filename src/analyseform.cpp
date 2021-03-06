/*******************************************************************
 This file is part of Synkron
 Copyright (C) 2005-2011 Matus Tomlein (matus.tomlein@gmail.com)

 Synkron is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public Licence
 as published by the Free Software Foundation; either version 2
 of the Licence, or (at your option) any later version.

 Synkron is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public Licence for more details.

 You should have received a copy of the GNU General Public Licence
 along with Synkron; if not, write to the Free Software Foundation,
 Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
********************************************************************/

#include "analyseform.h"
#include "ui_analyseform.h"

#include "folders.h"
#include "folder.h"
#include "folderactiongroup.h"
#include "analysefile.h"
#include "analysetreewidgetitem.h"
#include "abstractsyncpage.h"
#include "exceptionbundle.h"
#include "exceptiongroup.h"

#include <QTimer>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QDir>

AnalyseForm::AnalyseForm(AbstractSyncPage * page, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AnalyseForm)
{
    ui->setupUi(this);
    current_level_item = NULL;
    current_sf = NULL;
    this->page = page;

    QMenu * open_menu = new QMenu;
    ui->open_btn->setMenu(open_menu);

    QMenu * bl_menu = new QMenu;
    ui->blacklist_btn->setMenu(bl_menu);

    ui->folder_items_status_table->hide();
    num_copy_item = new QTableWidgetItem;
    ui->folder_items_status_table->setItem(0, 0, num_copy_item);
    num_delete_item = new QTableWidgetItem;
    ui->folder_items_status_table->setItem(2, 0, num_delete_item);
    num_update_item = new QTableWidgetItem;
    ui->folder_items_status_table->setItem(1, 0, num_update_item);

    ui->tree->setExpandsOnDoubleClick(false);
    QObject::connect(ui->tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(treeItemDoubleClicked(QTreeWidgetItem*,int)));
    QObject::connect(ui->tree, SIGNAL(itemSelectionChanged()), this, SLOT(treeItemSelectionChanged()));
    QObject::connect(open_menu, SIGNAL(aboutToShow()), this, SLOT(aboutToShowOpenMenu()));
    QObject::connect(open_menu, SIGNAL(triggered(QAction*)), this, SLOT(openSelected(QAction*)));
    QObject::connect(bl_menu, SIGNAL(aboutToShow()), this, SLOT(aboutToShowBlacklistMenu()));
    QObject::connect(bl_menu, SIGNAL(triggered(QAction*)), this, SLOT(blacklistSelected(QAction*)));
    QObject::connect(ui->sync_btn, SIGNAL(clicked()), this, SLOT(syncSelected()));

    QObject::connect(page, SIGNAL(analysisStarted()), this, SLOT(analysisStarted()), Qt::QueuedConnection);
    QObject::connect(page, SIGNAL(analysisFinished(AnalyseFile*)), this, SLOT(syncFileReceived(AnalyseFile*)), Qt::QueuedConnection);
}

AnalyseForm::~AnalyseForm()
{
    delete ui;
}

void AnalyseForm::analysisStarted()
{
    ui->tree->clear();
    sf_queue.clear();
    current_level_item = NULL;

    Folders * folders = page->foldersObject();

    ui->folders_table->clearContents();
    ui->folders_table->setRowCount(0);
    for (int i = 0; i < folders->count(); ++i) {
        ui->folders_table->insertRow(ui->folders_table->rowCount());
        ui->folders_table->setItem(ui->folders_table->rowCount() - 1, 0, new QTableWidgetItem(folders->at(i)->label()));
        ui->folders_table->setItem(ui->folders_table->rowCount() - 1, 1, new QTableWidgetItem);
        ui->folders_table->setRowHeight(ui->folders_table->rowCount() - 1, 22);
    }
}

void AnalyseForm::syncFileReceived(AnalyseFile * sf)
{
    sf_queue.clear();

    loadSyncFile(sf);
    if (ui->tree->topLevelItemCount())
        updateSelectedInfo((AnalyseTreeWidgetItem *) ui->tree->topLevelItem(0));
}

void AnalyseForm::loadSyncFile(AnalyseFile * parent_sf)
{
    loadSyncFile(nextLevelItem(parent_sf));
}

void AnalyseForm::loadSyncFile(AnalyseTreeWidgetItem * root_item)
{
    AnalyseFile * parent_sf = root_item->syncFile();

    for (int i = 0; i < parent_sf->childCount(); ++i) {
        if (parent_sf->numNotSynced() || !page->value("analyse_changed_only").toBool())
            new AnalyseTreeWidgetItem((AnalyseFile *) parent_sf->childAt(i), root_item);
    }

    sf_queue << parent_sf;
    root_item->sortChildren(0, Qt::AscendingOrder);
    root_item->setExpanded(true);
}

void AnalyseForm::removeItemChildren(AnalyseTreeWidgetItem * item)
{
    if (item) {
        for (int i = item->childCount() - 1; i >= 0; --i) {
            delete item->takeChild(i);
        }
    }
}

void AnalyseForm::treeItemDoubleClicked(QTreeWidgetItem * qitem, int)
{
    if (!sf_queue.count())
        return;

    AnalyseTreeWidgetItem * item = (AnalyseTreeWidgetItem *) qitem;

    if (!item->syncFile()->isDir())
        return;

    if (item->childCount() == 0 && item->syncFile() != sf_queue.last()) {
        loadSyncFile(item->syncFile());
    } else {
        if (item == current_level_item)
            return;

        sf_queue.takeLast();
        while ((current_level_item = (AnalyseTreeWidgetItem *) current_level_item->parent())) {
            removeItemChildren(current_level_item);
            sf_queue.takeLast();
            if (current_level_item == item) {
                loadSyncFile(item);
                break;
            }
        }
    }
}

AnalyseTreeWidgetItem * AnalyseForm::nextLevelItem(AnalyseFile * sf)
{
    if (!current_level_item) {
        current_level_item = new AnalyseTreeWidgetItem(sf, ui->tree);
    } else {
        removeItemChildren(current_level_item);
        current_level_item = new AnalyseTreeWidgetItem(sf, current_level_item);
    }
    return current_level_item;
}

void AnalyseForm::treeItemSelectionChanged()
{
    if (ui->tree->selectedItems().count())
        updateSelectedInfo((AnalyseTreeWidgetItem *) ui->tree->selectedItems().first());
}

void AnalyseForm::updateSelectedInfo(AnalyseTreeWidgetItem * item)
{
    AnalyseFile * sf = item->syncFile();
    current_sf = sf;

    QStringList rel_path = relativePath(sf);
    ui->rel_path_le->clear();
    ui->rel_path_le->setText(rel_path.join("/"));

    ui->folder_chb->setChecked(sf->isDir());
    num_copy_item->setText(QVariant(sf->numNotFound()).toString());
    num_delete_item->setText(QVariant(sf->numDeleted()).toString());
    num_update_item->setText(QVariant(sf->numObsolete()).toString());

    ui->folder_items_status_table->setHidden(!(sf->isDir() && sf->numNotSynced()));

    Folders * folders = page->foldersObject();
    QTableWidgetItem * folder_item;
    for (int i = 0; i < folders->count(); ++i) {
        folder_item = ui->folders_table->item(i, 1);
        switch (sf->fileStatusInFolder(folders->at(i)->index())) {
        case AnalyseFile::OK:
            folder_item->setText(tr("OK"));
            break;

        case AnalyseFile::Obsolete:
            folder_item->setText(tr("Obsolete"));
            break;

        case AnalyseFile::Deleted:
            folder_item->setText(tr("Deleted"));
            break;

        default:
            folder_item->setText(tr("Not found"));
            break;
        }
    }
}

void AnalyseForm::openSelected(QAction * action)
{
    int i = ui->open_btn->menu()->actions().indexOf(action);

    if (i < 0 || !current_sf)
        return;

    QStringList path = relativePath(current_sf);
    path.prepend(page->foldersObject()->at(i)->path());

    QDesktopServices::openUrl(QUrl::fromLocalFile(path.join("/")));
}

void AnalyseForm::aboutToShowOpenMenu()
{
    QMenu * menu = ui->open_btn->menu();
    menu->clear();

    Folders * folders = page->foldersObject();
    for (int i = 0; i < folders->count(); ++i) {
        menu->addAction(folders->at(i)->label());
    }
}

void AnalyseForm::blacklistSelected(QAction * action)
{
    ExceptionBundle * bundle = page->exceptionBundleById(action->data().toInt());
    ExceptionGroup * group;
    if (current_sf->isDir())
        group = bundle->groupByType(ExceptionGroup::FolderBlacklist);
    else
        group = bundle->groupByType(ExceptionGroup::FileBlacklist);

    QStringList path = relativePath(current_sf);
    path.prepend(page->foldersObject()->first()->path());

    group->addItem(path.join("/"));

    if (page->exceptionBundleChecked(bundle->index())) {
        delete current_sf;
        delete ui->tree->selectedItems().first();
    }
}

void AnalyseForm::aboutToShowBlacklistMenu()
{
    QMenu * menu = ui->blacklist_btn->menu();
    menu->clear();

    QAction * action;

    for (int i = 0; i < page->exceptionBundleCount(); ++i) {
        ExceptionBundle * bundle = page->exceptionBundleAt(i);
        action = new QAction(menu);
        action->setText(bundle->name());
        action->setData(bundle->index());
        menu->addAction(action);
    }
}

QStringList AnalyseForm::relativePath(AnalyseFile * sf)
{
    QStringList rel_path;

    if (sf == sf_queue.at(0))
        return rel_path;

    for (int i = 1; i < sf_queue.count(); ++i) {
        if (sf == sf_queue.at(i))
            break;
        rel_path << sf_queue.at(i)->getName();
    }
    rel_path << sf->getName();

    return rel_path;
}

void AnalyseForm::syncSelected()
{
    QList<QTreeWidgetItem *> selected = ui->tree->selectedItems();
    if (!selected.count())
        return;

    QStringList rel_path = relativePath(sf_queue.last());

    AnalyseFile * sf, * sf_i;
    sf = new AnalyseFile("/");

    for (int i = 0; i < selected.count(); ++i) {
        sf_i = ((AnalyseTreeWidgetItem *) selected.at(i))->syncFile();

        for (int i = 0; i < sf_queue.count(); ++i) {
            if (sf_queue.at(i) == sf_i) {
                delete sf;
                sf = sf_i;
                rel_path = relativePath(sf);
                sf_i = NULL;
            }
        }

        if (!sf_i)
            break;

        sf->addSyncFile((SyncFile *) sf_i);
    }

    FolderActionGroup * sync_fag = new FolderActionGroup;
    Folders * folders = page->foldersObject();
    QDir dir;
    QString rel_path_str = rel_path.join("/");
    for (int i = 0; i < folders->count(); ++i) {
        dir.setPath(folders->at(i)->path());
        sync_fag->insert(folders->at(i)->index(), dir.absoluteFilePath(rel_path_str));
    }

    if (sf)
        page->startSync(sf, sync_fag);
}
