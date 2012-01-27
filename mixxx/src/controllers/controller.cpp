/***************************************************************************
                             controller.cpp
                           Controller Class
                           ----------------
    begin                : Sat Apr 30 2011
    copyright            : (C) 2011 Sean M. Pappalardo
    email                : spappalardo@mixxx.org

***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include <qapplication.h>   // For command line arguments
#include "widget/wwidget.h"    // FIXME: This should be xmlparse.h
#include "controller.h"
#include "defs_controllers.h"

Controller::Controller() : QObject()
{
    m_bIsOutputDevice = false;
    m_bIsInputDevice = false;
    m_bIsOpen = false;
    m_pControllerEngine = NULL;

    // Get --controllerDebug command line option
    QStringList commandLineArgs = QApplication::arguments();
    m_bDebug = commandLineArgs.contains("--controllerDebug", Qt::CaseInsensitive);
}

Controller::~Controller()
{
}

void Controller::startEngine()
{
    if (debugging()) qDebug() << "  Starting engine";
    if (m_pControllerEngine != NULL) {
        qWarning() << "Controller: Engine already exists! Restarting:";
        stopEngine();
    }
    m_pControllerEngine = new ControllerEngine(this);
}

void Controller::stopEngine()
{
    if (debugging()) qDebug() << "  Shutting down engine";
    if (m_pControllerEngine == NULL) {
        qWarning() << "Controller::stopEngine(): No engine exists!";
        return;
    }
    m_pControllerEngine->gracefulShutdown();
    delete m_pControllerEngine;
    m_pControllerEngine = NULL;
}

/* loadPreset()
* Overloaded function for convenience, uses the default device path
* @param forceLoad Forces the preset to be loaded, regardless of whether or not the controller id
*        specified within matches the name of this Controller.
*/
void Controller::loadPreset(bool forceLoad) {
    loadPreset(DEFAULT_DEVICE_PRESET, forceLoad);
}

/* loadPreset(QString)
* Overloaded function for convenience
* @param path The path to a controller preset XML file.
* @param forceLoad Forces the preset to be loaded, regardless of whether or not the controller id
*        specified within matches the name of this Controller.
*/
void Controller::loadPreset(QString path, bool forceLoad) {
    qDebug() << "Loading controller preset from" << path;
    loadPreset(WWidget::openXMLFile(path, "controller"), forceLoad);
}

/* loadPreset(QDomElement)
* Loads a controller preset from a QDomElement structure.
* @param root The root node of the XML document for the MIDI mapping.
* @param forceLoad Forces the preset to be loaded, regardless of whether or not the controller id
*        specified within matches the name of this Controller.
*/
void Controller::loadPreset(QDomElement root, bool forceLoad) {
    //qDebug() << QString("Controller: loadPreset() called in thread ID=%1").arg(this->thread()->currentThreadId(),0,16);
    
    if (root.isNull()) return;

    m_scriptFileNames.clear();
    m_scriptFunctionPrefixes.clear();
    
    // For each controller in the DOM
    QDomElement controller = root.firstChildElement("controller");
    
    // For each controller in the preset XML...
    //(Only parse the <controller> block if its id matches our device name, otherwise
    //keep looking at the next controller blocks....)
    QString device;
    while (!controller.isNull()) {
        // Get deviceid
        device = controller.attribute("id","");
        if (device != m_sDeviceName && !forceLoad) {
            controller = controller.nextSiblingElement("controller");
        }
        else
            break;
    }
    
    if (!controller.isNull()) {
        
        qDebug() << device << " settings found";
        // Build a list of script files to load
        
        QDomElement scriptFile = controller.firstChildElement("scriptfiles").firstChildElement("file");
        
        // Default currently required file
        addScriptFile(REQUIRED_SCRIPT_FILE,"");
        
        // Look for additional ones
        while (!scriptFile.isNull()) {
            QString functionPrefix = scriptFile.attribute("functionprefix","");
            QString filename = scriptFile.attribute("filename","");
            addScriptFile(filename, functionPrefix);
            
            scriptFile = scriptFile.nextSiblingElement("file");
        }
    }
}   // END loadPreset(QDomElement)

/* applyPreset()
* Initializes the controller engine
*/
void Controller::applyPreset() {
    qDebug() << "Controller::applyPreset()";

    // Load the script code into the engine
    QStringList scriptFunctions;
    if (m_pControllerEngine != NULL) {
        scriptFunctions = m_pControllerEngine->getScriptFunctions();
        if (scriptFunctions.isEmpty() && m_scriptFileNames.isEmpty()) {
            qWarning() << "No script functions available! Did the XML file(s) load successfully? See above for any errors.";
        }
        else {
            if (scriptFunctions.isEmpty()) m_pControllerEngine->loadScriptFiles(m_scriptFileNames);
            m_pControllerEngine->initializeScripts(m_scriptFunctionPrefixes);
        }
    }
    else qWarning() << "Controller::applyPreset(): No engine exists!";
}

/* addScriptFile(QString,QString)
* Adds an entry to the list of script file names & associated list of function prefixes
* @param filename Name of the XML file to add
* @param functionprefix Function prefix to add
*/
void Controller::addScriptFile(QString filename, QString functionprefix) {
    m_scriptFileNames.append(filename);
    m_scriptFunctionPrefixes.append(functionprefix);
}


void Controller::send(QList<int> data, unsigned int length) {

    // If you change this implementation, also change it in HidController::send()
    //  (That function is required due to HID devices having report IDs)
    
    unsigned char * msg;
    msg = new unsigned char [length];

    for (unsigned int i=0; i<length; i++) {
        msg[i] = data.at(i);
//         qDebug() << "msg" << i << "=" << msg[i] << ", data=" << data.at(i);
    }

    send(msg,length);
    delete[] msg;
}

void Controller::send(unsigned char data[], unsigned int length) {
    qWarning() << "Error: data sending not yet implemented for this API or platform!";
}

void Controller::receive(const unsigned char data[], unsigned int length) {

    if (debugging()) {
        // Formatted packet display
        QString message = m_sDeviceName+": "+length+" bytes:\n";
        for(uint i=0; i<length; i++) {
            QString spacer=" ";
            if ((i+1) % 4 == 0) spacer="  ";
            if ((i+1) % 16 == 0) spacer="\n";
            message += QString("  %1%2")
                        .arg(data[i], 2, 16, QChar('0')).toUpper()
                        .arg(spacer);
        }
        qDebug()<< message;
    }
    
    QString function = m_sDeviceName + ".incomingData";

    if (!m_pControllerEngine->execute(function, data, length)) {
        qWarning() << "Controller: Invalid script function" << function;
    }
    return;
}
