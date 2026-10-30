/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEEPASSX_DETAILSWIDGET_H
#define KEEPASSX_DETAILSWIDGET_H

#include "config-keepassx.h"
#include "gui/DatabaseWidget.h"

namespace Ui
{
    class EntryPreviewWidget;
}

class QFrame;
class QGridLayout;
class QLabel;
class QLayoutItem;
class QScrollArea;
class QTabWidget;
class QTextEdit;
class QVBoxLayout;

class EntryPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EntryPreviewWidget(QWidget* parent = nullptr);
    ~EntryPreviewWidget() override;

public slots:
    void setEntry(Entry* selectedEntry);
    void setGroup(Group* selectedGroup);
    void setDatabaseMode(DatabaseWidget::Mode mode);
    void refresh();
    void clear();

signals:
    void entryUrlActivated(Entry* entry);
    void copyTextRequested(const QString& text);

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void updateEntryHeaderLine();
    void updateEntryTotp();
    void updateEntryGeneralTab();
    void updateEntryAdvancedTab();
    void updateEntryAutotypeTab();
    void setUsernameVisible(bool state);
    void setPasswordVisible(bool state);
    void setEntryNotesVisible(bool state);
    void setGroupNotesVisible(bool state);
    void setNotesVisible(QTextEdit* notesWidget, const QString& notes, bool state);

    void updateGroupHeaderLine();
    void updateGroupGeneralTab();
    void updateGroupSharingTab();

    void updateTotpLabel();
    void updateTabIndexes();
    void openEntryUrl();

private:
    // Shows the pages of a tab widget stacked below each other with headings instead of tabs
    struct SectionsView
    {
        struct Section
        {
            QWidget* page;
            QString title;
            bool enabled;
            QLabel* heading;
            QFrame* separator;
        };

        QTabWidget* tabWidget = nullptr;
        QScrollArea* scrollArea = nullptr;
        QVBoxLayout* layout = nullptr;
        QList<Section> sections;
        bool active = false;
    };

    struct GridItemPosition
    {
        QLayoutItem* item;
        int row;
        int column;
        int rowSpan;
        int columnSpan;
    };

    void initSectionsView(SectionsView& view, QTabWidget* tabWidget);
    void setSectionsMode(SectionsView& view, bool enabled);
    void applySectionsMode();
    static void fitTextEditsToContents(QWidget* page, bool enabled);
    static QLayoutItem* gridItem(QGridLayout* grid, const QObject* object);
    static QList<GridItemPosition> moveGridItems(QGridLayout* grid, const QList<GridItemPosition>& targets);
    void setEntryListLayout(bool enabled);
    void fitAttributesTableToContents();
    void setTabEnabled(QTabWidget* tabWidget, QWidget* widget, bool enabled);

    static QString hierarchy(const Group* group, const QString& title);

    const QScopedPointer<Ui::EntryPreviewWidget> m_ui;
    bool m_locked;
    QPointer<Entry> m_currentEntry;
    QPointer<Group> m_currentGroup;
    QTimer m_totpTimer;
    quint8 m_selectedTabEntry;
    quint8 m_selectedTabGroup;
    SectionsView m_entrySections;
    SectionsView m_groupSections;
    QList<GridItemPosition> m_generalGridRestore;
    QList<GridItemPosition> m_advancedGridRestore;
};

#endif // KEEPASSX_DETAILSWIDGET_H
