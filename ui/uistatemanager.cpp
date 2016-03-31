/*
 * This file is part of GammaRay, the Qt application inspection and
 * manipulation tool.
 *
 * Copyright (C) 2014-2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
 * Author: Filipe Azevedo <filipe.azevedo@kdab.com>
 *
 * Licensees holding valid commercial KDAB GammaRay licenses may use this file in
 * accordance with GammaRay Commercial License Agreement provided with the Software.
 *
 * Contact info@kdab.com if any conditions of this licensing are not clear to you.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "uistatemanager.h"
#include "deferredtreeview.h"
#include "common/settempvalue.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QMainWindow>
#include <QSplitter>
#include <QHeaderView>
#include <QSettings>
#include <QEvent>
#include <QTimer>
#include <QDebug>

#define WIDGET_CUSTOMIZED "customized"

using namespace GammaRay;

namespace {
QAbstractItemView *headerView(QHeaderView *header) {
    QWidget *view = header->parentWidget();

    while (!view->inherits("QAbstractItemView")) {
        view = view->parentWidget();
    }

    return qobject_cast<QAbstractItemView *>(view);
}

void distributeSpace(QList<int> &sizes, int size, int handleSize) {
  QList<QList<int>::Iterator> its;
  int usedSpace = 0;

  for (auto it = sizes.begin(), end = sizes.end(); it != end; ++it) {
    if ((*it) == -1) {
      its << it;
    } else {
      usedSpace += (*it);
    }
  }

  if (!its.isEmpty()) {
    const int freeSpace = size - usedSpace - (sizes.count() * handleSize) - handleSize;
    const int space = freeSpace / its.count();
    for (auto it = its.begin(), end = its.end(); it != end; ++it) {
      (*(*it)) = space;
    }
  }
}

typedef QList<QSplitter *> QSplitterList;
typedef QList<QHeaderView *> QHeaderViewList;
}

UIStateManager::UIStateManager(QWidget *widget)
  : QObject(widget)
  , m_widget(widget)
  , m_stateSettings(new QSettings("KDAB", "GammaRay", this))
  , m_initialized(false)
  , m_resizing(false)
{
  Q_ASSERT(m_widget);
  m_widget->installEventFilter(this);
}

UIStateManager::~UIStateManager()
{
}

QList<QSplitter *> UIStateManager::splitters() const
{
  return m_widget->findChildren<QSplitter *>();
}

QList<QHeaderView *> UIStateManager::headers() const
{
  return m_widget->findChildren<QHeaderView *>();
}

void UIStateManager::setup()
{
  if (!checkWidget(m_widget)) {
    return;
  }

  m_initialized = true;
  m_stateSettings->beginGroup(QString::fromLatin1("UiState/%1").arg(m_widget->objectName()));

  // Make sure objects names are unique.
  // Also make the comparison lower case as some QSettings implementations
  // are case sensitive.
  QSet<QString> knownNames;

  knownNames << m_widget->objectName().toLower();

  foreach (QSplitter *splitter, splitters()) {
    if (!checkWidget(splitter)) {
      continue;
    }

    const QString name = splitter->objectName().toLower();

    if (knownNames.contains(name)) {
      qWarning() << Q_FUNC_INFO << "Duplicate widget name" << name << "in widget" << m_widget;
      continue;
    }

    knownNames << name;

    connect(splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(widgetCustomized()), Qt::UniqueConnection);
  }

  foreach (QHeaderView *header, headers()) {
    if (!checkWidget(header)) {
      continue;
    }

    const QString name = header->objectName().toLower();

    if (knownNames.contains(name)) {
      qWarning() << Q_FUNC_INFO << "Duplicate widget name" << name << "in widget" << m_widget;
      continue;
    }

    knownNames << name;

    connect(header, SIGNAL(sectionResized(int,int,int)), this, SLOT(widgetCustomized()), Qt::UniqueConnection);
    connect(header, SIGNAL(sectionCountChanged(int,int)), this, SLOT(headerSectionCountChanged()), Qt::UniqueConnection);

    QAbstractItemView *view = headerView(header);
    view->removeEventFilter(this);
    view->installEventFilter(this);
  }

  restoreState();
}

void UIStateManager::restoreState()
{
  restoreWindowState();
  restoreSplitterState();
  restoreHeaderState();
}

void UIStateManager::saveState()
{
  saveWindowState();
  saveSplitterState();
  saveHeaderState();
}

bool UIStateManager::eventFilter(QObject *object, QEvent *event)
{
  if (object == m_widget) {
    if (event->type() == QEvent::Hide) {
      if (m_initialized) {
        saveState();
      }
    }
  }

  const bool result = QObject::eventFilter(object, event);

  if (object == m_widget) {
    if (event->type() == QEvent::Show) {
      if (!m_initialized) {
        setup();
      }
    }
  }

  if (event->type() == QEvent::Resize) {
    if (m_initialized && !m_resizing) {
      widgetResized(qobject_cast<QWidget *>(object));
    }
  }

  return result;
}

UISizeVector UIStateManager::defaultSizes(QSplitter *splitter) const
{
  return checkWidget(splitter) ? m_defaultSplitterSizes.value(splitter->objectName()) : UISizeVector();
}

void UIStateManager::setDefaultSizes(QSplitter *splitter, const UISizeVector &defaultSizes)
{
  if (checkWidget(splitter)) {
    m_defaultSplitterSizes[splitter->objectName()] = defaultSizes;
  }
}

UISizeVector UIStateManager::defaultSizes(QHeaderView *header) const
{
  return checkWidget(header) ? m_defaultHeaderSizes.value(header->objectName()) : UISizeVector();
}

void UIStateManager::setDefaultSizes(QHeaderView *header, const UISizeVector &defaultSizes)
{
  if (checkWidget(header)) {
    m_defaultHeaderSizes[header->objectName()] = defaultSizes;
  }
}

void UIStateManager::reset()
{
  m_initialized = false;
  m_stateSettings->endGroup();
  setup();
}

QString UIStateManager::widgetGeometryKey(QWidget *widget) const
{
  return QString::fromLatin1("%1Geometry").arg(widget->objectName());
}

QString UIStateManager::widgetStateKey(QWidget *widget) const
{
  return QString::fromLatin1("%1State").arg(widget->objectName());
}

bool UIStateManager::checkWidget(QWidget *widget) const
{
  if (widget->objectName().isEmpty()) {
    qWarning() << Q_FUNC_INFO << "Widget with no name" << widget << "in widget" << m_widget;
    return false;
  }

  return true;
}

int UIStateManager::percentToInt(const QString &size) const
{
  return size.left(size.length() -1).toInt();
}

void UIStateManager::restoreWindowState()
{
  QMainWindow *window = qobject_cast<QMainWindow *>(m_widget);

  if (window) {
    const QByteArray geometry = m_stateSettings->value(widgetGeometryKey(m_widget)).toByteArray();
    const QByteArray state = m_stateSettings->value(widgetStateKey(m_widget)).toByteArray();

    if (geometry.isEmpty()) {
      const QRect area = qApp->desktop()->availableGeometry(QCursor::pos());
      QRect rect(QPoint(), QSize(1024, 768));
      rect.moveCenter(area.center());
      m_widget->setGeometry(rect);
    } else {
      if (!m_resizing) {
        window->restoreGeometry(geometry);
        window->restoreState(state);
      }
    }
  }
}

void UIStateManager::saveWindowState()
{
  QMainWindow *window = qobject_cast<QMainWindow *>(m_widget);

  if (window) {
    m_stateSettings->setValue(widgetGeometryKey(m_widget), window->saveGeometry());
    m_stateSettings->setValue(widgetStateKey(m_widget), window->saveState());
  }
}

void UIStateManager::restoreSplitterState(QSplitter *splitter)
{
  const QSplitterList splitters = splitter ? (QSplitterList() << splitter) : this->splitters();
  foreach (QSplitter *splitter, splitters) {
    if (!checkWidget(splitter)) {
      continue;
    }

    const QByteArray state = m_stateSettings->value(widgetStateKey(splitter)).toByteArray();

    if (state.isEmpty()) {
      const UISizeVector defaultSizes = this->defaultSizes(splitter);

      if (!defaultSizes.isEmpty()) {
        Q_ASSERT(defaultSizes.count() == splitter->count());

        QList<int> sizes;
        sizes.reserve(defaultSizes.count());

        foreach (const QVariant &size, defaultSizes) {
          switch (size.type()) {
          case QVariant::Int: { // Pixels
            sizes << size.toInt();
            break;
          }

          case QVariant::String: { // Percent
            int value = percentToInt(size.toString());
            if (value == -1) {
              sizes << value;
            } else if (splitter->orientation() == Qt::Horizontal) {
              sizes << splitter->width() * value / 100;
            } else {
              sizes << splitter->height() * value / 100;
            }
            break;
          }

          default:
            Q_ASSERT(false);
          }
        }

        distributeSpace(sizes, splitter->orientation() == Qt::Horizontal ? splitter->width() : splitter->height(),
                        splitter->handleWidth());
        splitter->setSizes(sizes);
      }
    } else {
      if (!m_resizing) {
        splitter->restoreState(state);
        splitter->setProperty(WIDGET_CUSTOMIZED, true);
      }
    }
  }
}

void UIStateManager::saveSplitterState(QSplitter *splitter)
{
  const QSplitterList splitters = splitter ? (QSplitterList() << splitter) : this->splitters();
  foreach (QSplitter *splitter, splitters) {
    if (!checkWidget(splitter) || !splitter->property(WIDGET_CUSTOMIZED).toBool()) {
      continue;
    }

    m_stateSettings->setValue(widgetStateKey(splitter), splitter->saveState());
  }
}

void UIStateManager::restoreHeaderState(QHeaderView *header)
{
  const QHeaderViewList headers = header ? (QHeaderViewList() << header) : this->headers();
  foreach (QHeaderView *header, headers) {
    if (!checkWidget(header) || header->count() == 0) {
      continue;
    }

    const QByteArray state = m_stateSettings->value(widgetStateKey(header)).toByteArray();

    if (state.isEmpty()) {
      const UISizeVector defaultSizes = this->defaultSizes(header);
      QAbstractItemView *view = headerView(header);

      if (!defaultSizes.isEmpty()) {
        Q_ASSERT(defaultSizes.count() == header->count());

        QList<int> sizes;
        sizes.reserve(defaultSizes.count());

        int i = 0;
        for (auto it = defaultSizes.constBegin(), end = defaultSizes.constEnd(); it != end; ++it) {
          int size = 0;

          switch ((*it).type()) {
          case QVariant::Int: { // Pixels
            size = (*it).toInt();
            break;
          }

          case QVariant::String: { // Percent
            size = percentToInt((*it).toString());
            if (size == -1) {
            } else if (header->orientation() == Qt::Horizontal) {
              size = view->width() * size / 100;
            } else {
              size = view->height() * size / 100;
            }
            break;
          }

          default:
            Q_ASSERT(false);
          }

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
          switch (header->sectionResizeMode(i)) {
#else
          switch (header->resizeMode(i)) {
#endif
          case QHeaderView::Interactive:
          case QHeaderView::Fixed:
            header->resizeSection(i, size == -1 ? header->sectionSizeHint(i) : size);
            break;

          default:
            break;
          }

          i++;
        }
      }
    } else {
      if (!m_resizing) {
        header->restoreState(state);
        header->setProperty(WIDGET_CUSTOMIZED, true);
      }
    }
  }
}

void UIStateManager::saveHeaderState(QHeaderView *header)
{
  const QHeaderViewList headers = header ? (QHeaderViewList() << header) : this->headers();
  foreach (QHeaderView *header, headers) {
    if (!checkWidget(header) || header->count() == 0 || !header->property(WIDGET_CUSTOMIZED).toBool()) {
      continue;
    }

    m_stateSettings->setValue(widgetStateKey(header), header->saveState());
  }
}

void UIStateManager::headerSectionCountChanged()
{
  restoreHeaderState(qobject_cast<QHeaderView *>(sender()));
}

void UIStateManager::widgetResized(QWidget *widget)
{
  Util::SetTempValue<bool> guard(m_resizing, true);

  if (widget == m_widget) {
    restoreSplitterState();
    restoreHeaderState();
  } else {
    foreach (QHeaderView *header, widget->findChildren<QHeaderView *>()) {
      restoreHeaderState(header);
    }
  }
}

void UIStateManager::widgetCustomized()
{
  QSplitter *splitter = qobject_cast<QSplitter *>(sender());
  HeaderView *header = qobject_cast<HeaderView *>(sender());

  if (splitter) {
    splitter->setProperty(WIDGET_CUSTOMIZED, true);
    saveSplitterState(splitter);
  } else if (header) {
    // There is no way to know if a resize is manual or programamtically...
    // So only cast against our HeaderView which has a isState() member.
    if (header->isState(HeaderView::ResizeSection)) {
      header->setProperty(WIDGET_CUSTOMIZED, true);
      saveHeaderState(header);
    }
  }
}
