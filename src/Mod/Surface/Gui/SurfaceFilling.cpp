/***************************************************************************
 *   Copyright (c) 2015 Balázs Bámer                                       *
 *                      Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"
#include <QAction>
#include <QMenu>
#include <QMessageBox>

#include <Mod/Surface/App/FillType.h>
#include <Gui/ViewProvider.h>
#include <Gui/Application.h>
#include <Gui/Document.h>
#include <Gui/Command.h>
#include <Gui/SelectionObject.h>
#include <Base/Console.h>
#include <Gui/Control.h>
#include <Gui/BitmapFactory.h>

#include "SurfaceFilling.h"
#include "ui_SurfaceFilling.h"


using namespace SurfaceGui;

PROPERTY_SOURCE(SurfaceGui::ViewProviderSurfaceFeature, PartGui::ViewProviderSpline)

namespace SurfaceGui {

bool EdgeSelection::allow(App::Document* , App::DocumentObject* pObj, const char* sSubName)
{
    // don't allow references to itself
    if (pObj == editedObject)
        return false;
    if (!pObj->isDerivedFrom(Part::Feature::getClassTypeId()))
        return false;
    if (!sSubName || sSubName[0] == '\0')
        return false;
    std::string element(sSubName);
    if (element.substr(0,4) != "Edge")
        return false;
    auto links = editedObject->BoundaryList.getSubListValues();
    for (auto it : links) {
        if (it.first == pObj) {
            for (auto jt : it.second) {
                if (jt == sSubName)
                    return !appendEdges;
            }
        }
    }

    return appendEdges;
}

// ----------------------------------------------------------------------------

void ViewProviderSurfaceFeature::setupContextMenu(QMenu* menu, QObject* receiver, const char* member)
{
    QAction* act;
    act = menu->addAction(QObject::tr("Edit filling"), receiver, member);
    act->setData(QVariant((int)ViewProvider::Default));
    PartGui::ViewProviderSpline::setupContextMenu(menu, receiver, member);
}

bool ViewProviderSurfaceFeature::setEdit(int ModNum)
{
    if (ModNum == ViewProvider::Default ) {
        // When double-clicking on the item for this sketch the
        // object unsets and sets its edit mode without closing
        // the task panel

        Surface::SurfaceFeature* obj =  static_cast<Surface::SurfaceFeature*>(this->getObject());

        Gui::TaskView::TaskDialog* dlg = Gui::Control().activeDialog();

        // start the edit dialog
        if (dlg) {
            TaskSurfaceFilling* tDlg = qobject_cast<TaskSurfaceFilling*>(dlg);
            if (tDlg)
                tDlg->setEditedObject(obj);
            Gui::Control().showDialog(dlg);
        }
        else {
            Gui::Control().showDialog(new TaskSurfaceFilling(this, obj));
        }
        return true;
    }
    else {
        return ViewProviderSpline::setEdit(ModNum);
    }
}

void ViewProviderSurfaceFeature::unsetEdit(int ModNum)
{
    if (ModNum == ViewProvider::Default) {
        // when pressing ESC make sure to close the dialog
        QTimer::singleShot(0, &Gui::Control(), SLOT(closeDialog()));
    }
    else {
        PartGui::ViewProviderSpline::unsetEdit(ModNum);
    }
}

QIcon ViewProviderSurfaceFeature::getIcon(void) const
{
    return Gui::BitmapFactory().pixmap("BSplineSurf");
}

// ----------------------------------------------------------------------------

SurfaceFilling::SurfaceFilling(ViewProviderSurfaceFeature* vp, Surface::SurfaceFeature* obj)
{
    ui = new Ui_SurfaceFilling();
    ui->setupUi(this);
    selectionMode = None;
    this->vp = vp;
    setEditedObject(obj);
}

/*
 *  Destroys the object and frees any allocated resources
 */
SurfaceFilling::~SurfaceFilling()
{
    // no need to delete child widgets, Qt does it all for us
    delete ui;
}

// stores object pointer, its old fill type and adjusts radio buttons according to it.
void SurfaceFilling::setEditedObject(Surface::SurfaceFeature* obj)
{
    editedObject = obj;
    long curtype = editedObject->FillType.getValue();
    switch(curtype)
    {
    case 1: // StretchStyle
        ui->fillType_stretch->setChecked(true);
        break;
    case 2: // CoonsStyle
        ui->fillType_coons->setChecked(true);
        break;
    case 3: // CurvedStyle
        ui->fillType_curved->setChecked(true);
        break;
    default:
        break;
    }

    auto objects = editedObject->BoundaryList.getValues();
    auto element = editedObject->BoundaryList.getSubValues();
    auto it = objects.begin();
    auto jt = element.begin();

    App::Document* doc = editedObject->getDocument();
    for (; it != objects.end() && jt != element.end(); ++it, ++jt) {
        QListWidgetItem* item = new QListWidgetItem(ui->listWidget);
        ui->listWidget->addItem(item);

        QString text = QString::fromLatin1("%1.%2")
                .arg(QString::fromUtf8((*it)->Label.getValue()))
                .arg(QString::fromStdString(*jt));
        item->setText(text);

        QList<QVariant> data;
        data << QByteArray(doc->getName());
        data << QByteArray((*it)->getNameInDocument());
        data << QByteArray(jt->c_str());
        item->setData(Qt::UserRole, data);
    }

    attachDocument(Gui::Application::Instance->getDocument(doc));
}

void SurfaceFilling::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
    else {
        QWidget::changeEvent(e);
    }
}

void SurfaceFilling::open()
{
    if (!Gui::Command::hasPendingCommand()) {
        std::string Msg("Edit ");
        Msg += editedObject->Label.getValue();
        Gui::Command::openCommand(Msg.c_str());
    }
}

void SurfaceFilling::slotUndoDocument(const Gui::Document&)
{
  //Gui::Command::doCommand(Gui::Command::Gui,"Gui.ActiveDocument.resetEdit()");
}

void SurfaceFilling::slotRedoDocument(const Gui::Document&)
{
  //Gui::Command::doCommand(Gui::Command::Gui,"Gui.ActiveDocument.resetEdit()");
}

bool SurfaceFilling::accept()
{
    selectionMode = None;
    Gui::Selection().rmvSelectionGate();

    int count = ui->listWidget->count();
    if (count > 4) {
        QMessageBox::warning(this,
            tr("Too many edges"),
            tr("The tool requires two, three or four edges"));
        return false;
    }
    else if (count < 2) {
        QMessageBox::warning(this,
            tr("Too less edges"),
            tr("The tool requires two, three or four edges"));
        return false;
    }

    if (editedObject->mustExecute())
        editedObject->recomputeFeature();
    if (!editedObject->isValid()) {
        QMessageBox::warning(this, tr("Invalid object"),
            QString::fromLatin1(editedObject->getStatusString()));
        return false;
    }

    Gui::Command::commitCommand();
    Gui::Command::doCommand(Gui::Command::Gui,"Gui.ActiveDocument.resetEdit()");
    Gui::Command::updateActive();
    return true;
}

bool SurfaceFilling::reject()
{
    selectionMode = None;
    Gui::Selection().rmvSelectionGate();

    Gui::Command::abortCommand();
    Gui::Command::doCommand(Gui::Command::Gui,"Gui.ActiveDocument.resetEdit()");
    Gui::Command::updateActive();
    return true;
}

void SurfaceFilling::on_fillType_stretch_clicked()
{
    long curtype = editedObject->FillType.getValue();
    if (curtype != 1) {
        editedObject->FillType.setValue(1);
        editedObject->recomputeFeature();
        if (!editedObject->isValid()) {
            Base::Console().Error("Surface filling: %s", editedObject->getStatusString());
        }
    }
}

void SurfaceFilling::on_fillType_coons_clicked()
{
    long curtype = editedObject->FillType.getValue();
    if (curtype != 2) {
        editedObject->FillType.setValue(2);
        editedObject->recomputeFeature();
        if (!editedObject->isValid()) {
            Base::Console().Error("Surface filling: %s", editedObject->getStatusString());
        }
    }
}

void SurfaceFilling::on_fillType_curved_clicked()
{
    long curtype = editedObject->FillType.getValue();
    if (curtype != 3) {
        editedObject->FillType.setValue(3);
        editedObject->recomputeFeature();
        if (!editedObject->isValid()) {
            Base::Console().Error("Surface filling: %s", editedObject->getStatusString());
        }
    }
}

void SurfaceFilling::on_buttonEdgeAdd_clicked()
{
    selectionMode = Append;
    Gui::Selection().addSelectionGate(new EdgeSelection(true, editedObject));
}

void SurfaceFilling::on_buttonEdgeRemove_clicked()
{
    selectionMode = Remove;
    Gui::Selection().addSelectionGate(new EdgeSelection(false, editedObject));
}

void SurfaceFilling::onSelectionChanged(const Gui::SelectionChanges& msg)
{
    if (selectionMode == None)
        return;

    if (msg.Type == Gui::SelectionChanges::AddSelection) {
        if (selectionMode == Append) {
            QListWidgetItem* item = new QListWidgetItem(ui->listWidget);
            ui->listWidget->addItem(item);

            Gui::SelectionObject sel(msg);
            QString text = QString::fromLatin1("%1.%2")
                    .arg(QString::fromUtf8(sel.getObject()->Label.getValue()))
                    .arg(QString::fromLatin1(msg.pSubName));
            item->setText(text);

            QList<QVariant> data;
            data << QByteArray(msg.pDocName);
            data << QByteArray(msg.pObjectName);
            data << QByteArray(msg.pSubName);
            item->setData(Qt::UserRole, data);

            auto objects = editedObject->BoundaryList.getValues();
            objects.push_back(sel.getObject());
            auto element = editedObject->BoundaryList.getSubValues();
            element.push_back(msg.pSubName);
            editedObject->BoundaryList.setValues(objects, element);
        }
        else {
            Gui::SelectionObject sel(msg);
            QList<QVariant> data;
            data << QByteArray(msg.pDocName);
            data << QByteArray(msg.pObjectName);
            data << QByteArray(msg.pSubName);
            for (int i=0; i<ui->listWidget->count(); i++) {
                QListWidgetItem* item = ui->listWidget->item(i);
                if (item && item->data(Qt::UserRole) == data) {
                    ui->listWidget->takeItem(i);
                    delete item;
                }
            }

            App::DocumentObject* obj = sel.getObject();
            std::string sub = msg.pSubName;
            auto objects = editedObject->BoundaryList.getValues();
            auto element = editedObject->BoundaryList.getSubValues();
            auto it = objects.begin();
            auto jt = element.begin();
            for (; it != objects.end() && jt != element.end(); ++it, ++jt) {
                if (*it == obj && *jt == sub) {
                    objects.erase(it);
                    element.erase(jt);
                    editedObject->BoundaryList.setValues(objects, element);
                    break;
                }
            }
        }

        editedObject->recomputeFeature();
    }
}

// ----------------------------------------------------------------------------

TaskSurfaceFilling::TaskSurfaceFilling(ViewProviderSurfaceFeature* vp, Surface::SurfaceFeature* obj)
{
    widget = new SurfaceFilling(vp, obj);
    widget->setWindowTitle(QObject::tr("Surface"));
    taskbox = new Gui::TaskView::TaskBox(
        Gui::BitmapFactory().pixmap("BezSurf"),
        widget->windowTitle(), true, 0);
    taskbox->groupLayout()->addWidget(widget);
    Content.push_back(taskbox);
}

TaskSurfaceFilling::~TaskSurfaceFilling()
{
    // automatically deleted in the sub-class
}

void TaskSurfaceFilling::setEditedObject(Surface::SurfaceFeature* obj)
{
    widget->setEditedObject(obj);
}

void TaskSurfaceFilling::open()
{
    widget->open();
}

bool TaskSurfaceFilling::accept()
{
    return widget->accept();
}

bool TaskSurfaceFilling::reject()
{
    return widget->reject();
}

}

#include "moc_SurfaceFilling.cpp"