/************************************************************************
**
**  Copyright (C) 2019-2020  Kevin B. Hendricks, Stratford, Ontario, Canada
**  Copyright (C) 2019-2020  Doug Massay
**
**  This file is part of PageEdit.
**
**  PageEdit is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  PageEdit is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with PageEdit.  If not, see <http://www.gnu.org/licenses/>.
**
*************************************************************************/

#include <iostream>

#include <QCoreApplication>
#include <QApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QStyleFactory>
#include <QStyle>
#include <QTextCodec>
#include <QTranslator>
#include <QMessageBox>
#include <QFileInfo>
#include <QDebug>

#ifdef Q_OS_MAC
# include <QFileDialog>
# include <QKeySequence>
# include <QAction>
extern void disableWindowTabbing();
extern void removeMacosSpecificMenuItems();
#endif

#include "MainApplication.h"
#include "MainWindow.h"
#include "Utility.h"
#include "AppEventFilter.h"
#include "SettingsStore.h"
#include "UILanguage.h"
#include "pageedit_constants.h"
#include "pageedit_exception.h"


// Creates a MainWindow instance depending
// on command line arguments
static MainWindow *GetMainWindow(const QStringList &arguments)
{
    // We use the first argument as the file to load after starting
    QString filepath;
    if (arguments.size() > 1 && Utility::IsFileReadable(arguments.at(1))) {
        filepath = arguments.at(1);
    }
    return new MainWindow(filepath);
}


#ifdef Q_OS_MAC
// implement file open for use when no mainwindows open
static void file_open()
{
    const QMap<QString, QString> load_filters = MainWindow::GetLoadFiltersMap();
    QStringList filters(load_filters.values());
    filters.removeDuplicates();
    QString filter_string = "";
    foreach(QString filter, filters) {
        filter_string += filter + ";;";
    }
    // "All Files (*.*)" is the default
    QString default_filter = load_filters.value("xhtml");
    QString filename = QFileDialog::getOpenFileName(0,
						  "Open File",
						  "~",
						  filter_string,
						  &default_filter
						  );
    if (!filename.isEmpty()) {
        MainWindow *w = GetMainWindow(QStringList() << "" << filename);
        w->show();
    }
}
#endif

#if !defined(Q_OS_WIN32) && !defined(Q_OS_MAC)
// Returns a QIcon with the PageEdit "PE" logo in various sizes
static QIcon GetApplicationIcon()
{
    QIcon app_icon;
    app_icon.addFile(":/icons/app_icon_32.png",  QSize(32, 32));
    app_icon.addFile(":/icons/app_icon_48.png",  QSize(48, 48));
    app_icon.addFile(":/icons/app_icon_128.png", QSize(128, 128));
    app_icon.addFile(":/icons/app_icon_256.png", QSize(256, 256));
    app_icon.addFile(":/icons/app_icon_512.png", QSize(512, 512));
    return app_icon;
}
#endif


void setupHighDPI()
{
    bool has_env_setting = false;
    QStringList env_vars;
    env_vars << "QT_ENABLE_HIGHDPI_SCALING" << "QT_SCALE_FACTOR_ROUNDING_POLICY"
             << "QT_AUTO_SCREEN_SCALE_FACTOR" << "QT_SCALE_FACTOR"
             << "QT_SCREEN_SCALE_FACTORS" << "QT_DEVICE_PIXEL_RATIO";
    foreach(QString v, env_vars) {
        if (!Utility::GetEnvironmentVar(v).isEmpty()) {
            has_env_setting = true;
            break;
        }
    }

    SettingsStore ss;
    int highdpi = ss.highDPI();
    if (highdpi == 1 || (highdpi == 0 && !has_env_setting)) {
        // Turning on Automatic High DPI scaling
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    } else if (highdpi == 2) {
        // Turning off Automatic High DPI scaling
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, false);
        foreach(QString v, env_vars) {
            bool irrel = qunsetenv(v.toUtf8().constData());
        }
    }
}


// The message handler installed to handle Qt messages
void MessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QString qt_log_entry;
    QString context_file;

    switch (type) {
        case QtDebugMsg:
            qt_log_entry = QString("Debug: %1").arg(message.toLatin1().constData());
            fprintf(stderr, "Debug: %s\n", message.toLatin1().constData());
            break;
        case QtInfoMsg:
            qt_log_entry = QString("Info: %1").arg(message.toLatin1().constData());
            fprintf(stderr, "Info: %s\n", message.toLatin1().constData());
            break;
        case QtWarningMsg:
            qt_log_entry = QString("Warning: %1").arg(message.toLatin1().constData());
            fprintf(stderr, "Warning: %s\n", message.toLatin1().constData());
            break;
        case QtCriticalMsg:
            qt_log_entry = QString("Critical: %1").arg(message.toLatin1().constData());
	    if (context.file) context_file = QString(context.file);
	    // screen out error messages from inspector / devtools
            if (!context_file.contains("devtools://devtools")) {
                //Utility::DisplayExceptionErrorDialog(QString("Critical: %1").arg(error_message));
                fprintf(stderr, "Critical: %s\n", message.toLatin1().constData());
	    }
            break;
        case QtFatalMsg:
            Utility::DisplayExceptionErrorDialog(QString("Fatal: %1").arg(QString(message)));
            abort();
    }

    // qDebug() prints to PAGEEDIT_DEBUG_LOGFILE environment variable.
    // User must have permissions to write to the location or no file will be created.
    QString pageedit_log_file;
    pageedit_log_file = Utility::GetEnvironmentVar("PAGEEDIT_DEBUG_LOGFILE");
    if (!pageedit_log_file.isEmpty()) {
        QFile outFile(pageedit_log_file);
        outFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        QTextStream ts(&outFile);
        ts << qt_log_entry << endl;
    }
}


QPalette getDarkPalette()
{
    // Dark palette for PageEdit
    QPalette darkPalette;

    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::Disabled, QPalette::Window, QColor(80, 80, 80));
    darkPalette.setColor(QPalette::WindowText, QColor(238, 238, 238));
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText,
                        QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Base, QColor(42, 42, 42));
    darkPalette.setColor(QPalette::Disabled, QPalette::Base, QColor(80, 80, 80));
    darkPalette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ToolTipText, QColor(238, 238, 238));
    darkPalette.setColor(QPalette::Text, QColor(238, 238, 238));
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    darkPalette.setColor(QPalette::Dark, QColor(35, 35, 35));
    darkPalette.setColor(QPalette::Shadow, QColor(20, 20, 20));
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, QColor(238, 238, 238));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText,
                        QColor(127, 127, 127));
    darkPalette.setColor(QPalette::BrightText, Qt::red);
    darkPalette.setColor(QPalette::Link, QColor(108, 180, 238));
    darkPalette.setColor(QPalette::LinkVisited, QColor(108, 180, 238));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
    darkPalette.setColor(QPalette::HighlightedText, QColor(238, 238, 238));
    darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText,
                        QColor(127, 127, 127));

    return darkPalette;
}


// Application entry point
int main(int argc, char *argv[])
{
#if !defined(Q_OS_WIN32) && !defined(Q_OS_MAC)
    QT_REQUIRE_VERSION(argc, argv, "5.9.0");
#else
    QT_REQUIRE_VERSION(argc, argv, "5.12.3");
#endif

#ifndef QT_DEBUG
    qInstallMessageHandler(MessageHandler);
#endif

    // Set application information for easier use of QSettings classes
    QCoreApplication::setOrganizationName("sigil-ebook");
    QCoreApplication::setOrganizationDomain("sigil-ebook.com");
    QCoreApplication::setApplicationName("pageedit");
    QCoreApplication::setApplicationVersion(QString(PAGEEDIT_VERSION));
#ifndef Q_OS_MAC
    setupHighDPI();
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QCoreApplication::setAttribute(Qt::AA_DisableShaderDiskCache);

    MainApplication app(argc, argv);

#ifdef Q_OS_MAC
    disableWindowTabbing();
    removeMacosSpecificMenuItems();
#endif

    // Install an event filter for the application
    // so we can catch OS X's file open events
    AppEventFilter *filter = new AppEventFilter(&app);
    app.installEventFilter(filter);

    QTextCodec::setCodecForLocale(QTextCodec::codecForName("utf8"));

    // set up for translations
    SettingsStore settings;

    // Setup the qtbase_ translator and load the translation for the selected language
    QTranslator qtbaseTranslator;
    const QString qm_name_qtbase = QString("qtbase_%1").arg(settings.uiLanguage());
    // Run though all locations and stop once we find and are able to load
    // an appropriate Qt base translation.
    foreach(QString path, UILanguage::GetPossibleTranslationPaths()) {
        if (QDir(path).exists()) {
	    if (qtbaseTranslator.load(qm_name_qtbase, path)) {
	        break;
	    }
        }
    }
    app.installTranslator(&qtbaseTranslator);

    // Setup the PageEdit translator and load the translation for the selected language
    QTranslator pageeditTranslator;
    const QString qm_name = QString("pageedit_%1").arg(settings.uiLanguage());
    // Run though all locations and stop once we find and are able to load
    // an appropriate translation.
    foreach(QString path, UILanguage::GetPossibleTranslationPaths()) {
        if (QDir(path).exists()) {
	    if (pageeditTranslator.load(qm_name, path)) {
	        break;
	    }
        }
    }
    app.installTranslator(&pageeditTranslator);

#ifndef Q_OS_MAC
#ifndef Q_OS_WIN32
    // Use platform themes/styles on Linux unless either Sigil or PageEdit's FORCE variable is set
    if (!force_pageedit_darkmode_palette.isEmpty() || !force_sigil_darkmode_palette.isEmpty()) {
        // Fusion style is fully dpi aware on Windows/Linux
        app.setStyle(QStyleFactory::create("fusion"));
        // qss stylesheet from resources
        QString dark_styles = Utility::ReadUnicodeTextFile(":/dark/win-dark-style.qss");
        app.setStyleSheet(dark_styles);
        app.setPalette(getDarkPalette());
    }
#else
    if (Utility::WindowsShouldUseDarkMode()) {
        // Fusion style is fully dpi aware on Windows/Linux
        app.setStyle(QStyleFactory::create("fusion"));
        // qss stylesheet from resources
        QString dark_styles = Utility::ReadUnicodeTextFile(":/dark/win-dark-style.qss");
        app.setStyleSheet(dark_styles);
        app.setPalette(getDarkPalette());
    }
#endif
#endif

    // Set ui font from preferences after dark theming
    QFont f = QFont(QApplication::font());
#ifdef Q_OS_WIN32
    if (f.family() == "MS Shell Dlg 2" && f.pointSize() == 8) {
        // Microsoft's recommended UI defaults
        f.setFamily("Segoe UI");
        f.setPointSize(9);
	QApplication::setFont(f);
    }
#elif defined(Q_OS_MAC)
    // Just in case
#else
    if (f.family() == "Sans Serif" && f.pointSize() == 9) {
        f.setPointSize(10);
	QApplication::setFont(f);
    }
#endif
    settings.setOriginalUIFont(f.toString());
    if (!settings.uiFont().isEmpty()) {
        QFont font;
        if (font.fromString(settings.uiFont()))
	    QApplication::setFont(font);
    }
#ifndef Q_OS_MAC
    // redo on a timer to ensure in all cases
    if (!settings.uiFont().isEmpty()) {
        QFont font;
        if (font.fromString(settings.uiFont())) {
	    QTimer::singleShot(0, [=]() {
		    QApplication::setFont(font);
		} );
        }
    }
#endif
    // End of UI font stuff

    // Check for existing qt_styles.qss in Prefs dir and load it if present
    QString qt_stylesheet_path = Utility::DefinePrefsDir() + "/qt_styles.qss";
    QFileInfo QtStylesheetInfo(qt_stylesheet_path);
    if (QtStylesheetInfo.exists() && QtStylesheetInfo.isFile() && QtStylesheetInfo.isReadable()) {
        QString qtstyles = Utility::ReadUnicodeTextFile(qt_stylesheet_path);
        app.setStyleSheet(app.styleSheet().append(qtstyles));
    }

    // Qt's setCursorFlashTime(msecs) (or the docs) are broken
    // According to the docs, setting a negative value should disable cursor blinking 
    // but instead just forces it to look for PlatformSpecific Themeable Hints to get 
    // a value which for Mac OS X is hardcoded to 1000 ms
    // This was the only way I could get Qt to disable cursor blinking on a Mac if desired
    if (qEnvironmentVariableIsSet("PAGEEDIT_DISABLE_CURSOR_BLINK")) {
      // qDebug() << "trying to disable text cursor blinking";
      app.setCursorFlashTime(0);
      // qDebug() << "cursorFlashTime: " << app.cursorFlashTime();
    }

// application icons linuxicons
#if !defined(Q_OS_WIN32) && !defined(Q_OS_MAC)
    app.setWindowIcon(GetApplicationIcon());
#if QT_VERSION >= 0x050700
    // Wayland needs this clarified in order to propery assign the icon 
    app.setDesktopFileName(QStringLiteral("pageedit.desktop"));
#endif
#endif

    QStringList arguments = QCoreApplication::arguments();

#ifdef Q_OS_MAC
    // now process main app events so that any startup 
    // FileOpen event will be processed for macOS
    QCoreApplication::processEvents();

    QString filepath = filter->getInitialFilePath();

    // if one found append it to argv for processing as normal
    if ((arguments.size() == 1) && !filepath.isEmpty()) {
      arguments << QFileInfo(filepath).absoluteFilePath();
    }

    if (filepath.isEmpty()) filter->setInitialFilePath(QString("placeholder"));

    // Work around QTBUG-62193 and QTBUG-65245 and others where menubar
    // menu items are lost under File and PageEdit menus and where
    // Quit menu gets lost when deleting other windows first

    // We Create and show a frameless translucent QMainWindow to hold
    // the menubar.  Note: macOS has a single menubar attached at 
    // the top of the screen that all mainwindows share.

    app.setQuitOnLastWindowClosed(false);

    Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint;
    QMainWindow * basemw = new QMainWindow(NULL, flags);
    basemw->setAttribute(Qt::WA_TranslucentBackground, true);

    QMenuBar *mac_menu = new QMenuBar(0);
    QMenu *file_menu = new QMenu("File");

    // Open
    QAction* open_action = new QAction("Open");
    open_action->setShortcut(QKeySequence("Ctrl+O"));
    QObject::connect(open_action, &QAction::triggered, file_open);
    file_menu ->addAction(open_action);

    // Quit - force add of a secondary quit menu to the file menu
    QAction* quit_action = new QAction("Quit");
    quit_action->setMenuRole(QAction::NoRole);
    quit_action->setShortcut(QKeySequence("Ctrl+Q"));
    QObject::connect(quit_action, &QAction::triggered, qApp->quit);
    file_menu ->addAction(quit_action);

    mac_menu->addMenu(file_menu);

    // Application specific quit menu
    // according to Qt docs this is the right way to add an application
    // quit menu - but it does not work and will still sometimes get lost
    mac_menu->addAction("quit");

    basemw->setMenuBar(mac_menu);
    basemw->show();
#endif

    MainWindow *widget = GetMainWindow(arguments);
    widget->show();
    return app.exec();
}
