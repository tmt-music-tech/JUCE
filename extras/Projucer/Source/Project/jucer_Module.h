/*
  ==============================================================================

   This file is part of the JUCE 6 technical preview.
   Copyright (c) 2017 - ROLI Ltd.

   You may use this code under the terms of the GPL v3
   (see www.gnu.org/licenses).

   For this technical preview, this file is not subject to commercial licensing.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#pragma once

#include "jucer_Project.h"
class ProjectExporter;
class ProjectSaver;

//==============================================================================
struct ModuleDescription
{
    ModuleDescription() = default;
    ModuleDescription (const File& folder);

    bool isValid() const                    { return getID().isNotEmpty(); }

    String getID() const                    { return moduleInfo [Ids::ID_uppercase].toString(); }
    String getVendor() const                { return moduleInfo [Ids::vendor].toString(); }
    String getVersion() const               { return moduleInfo [Ids::version].toString(); }
    String getName() const                  { return moduleInfo [Ids::name].toString(); }
    String getDescription() const           { return moduleInfo [Ids::description].toString(); }
    String getLicense() const               { return moduleInfo [Ids::license].toString(); }
    String getMinimumCppStandard() const    { return moduleInfo [Ids::minimumCppStandard].toString(); }
    String getPreprocessorDefs() const      { return moduleInfo [Ids::defines].toString(); }
    String getExtraSearchPaths() const      { return moduleInfo [Ids::searchpaths].toString(); }
    StringArray getDependencies() const;

    File getFolder() const                  { jassert (moduleFolder != File()); return moduleFolder; }
    File getHeader() const;

    File moduleFolder;
    var moduleInfo;
    URL url;
};

//==============================================================================
class LibraryModule
{
public:
    LibraryModule (const ModuleDescription&);

    bool isValid() const                    { return moduleInfo.isValid(); }
    String getID() const                    { return moduleInfo.getID(); }
    String getVendor() const                { return moduleInfo.getVendor(); }
    String getVersion() const               { return moduleInfo.getVersion(); }
    String getName() const                  { return moduleInfo.getName(); }
    String getDescription() const           { return moduleInfo.getDescription(); }
    String getLicense() const               { return moduleInfo.getLicense(); }
    String getMinimumCppStandard() const    { return moduleInfo.getMinimumCppStandard(); }

    File getFolder() const                  { return moduleInfo.getFolder(); }

    void writeIncludes (ProjectSaver&, OutputStream&);
    void addSettingsForModuleToExporter (ProjectExporter&, ProjectSaver&) const;
    void getConfigFlags (Project&, OwnedArray<Project::ConfigFlag>& flags) const;
    void findBrowseableFiles (const File& localModuleFolder, Array<File>& files) const;

    struct CompileUnit
    {
        File file;
        bool isCompiledForObjC = false, isCompiledForNonObjC = false;

        bool isNeededForExporter (ProjectExporter&) const;
        String getFilenameForProxyFile() const;
        static bool hasSuffix (const File&, const char*);
    };

    Array<CompileUnit> getAllCompileUnits (build_tools::ProjectType::Target::Type forTarget =
                                               build_tools::ProjectType::Target::unspecified) const;
    void findAndAddCompiledUnits (ProjectExporter&, ProjectSaver*, Array<File>& result,
                                  build_tools::ProjectType::Target::Type forTarget =
                                      build_tools::ProjectType::Target::unspecified) const;

    ModuleDescription moduleInfo;

private:
    void addSearchPathsToExporter (ProjectExporter&) const;
    void addDefinesToExporter (ProjectExporter&) const;
    void addCompileUnitsToExporter (ProjectExporter&, ProjectSaver&) const;
    void addLibsToExporter (ProjectExporter&) const;

    void addBrowseableCode (ProjectExporter&, const Array<File>& compiled, const File& localModuleFolder) const;

    mutable Array<File> sourceFiles;
    OwnedArray<Project::ConfigFlag> configFlags;
};

//==============================================================================
class AvailableModuleList
{
public:
    using ModuleIDAndFolder     = std::pair<String, File>;
    using ModuleIDAndFolderList = std::vector<ModuleIDAndFolder>;

    AvailableModuleList() = default;

    void scanPaths      (const Array<File>&);
    void scanPathsAsync (const Array<File>&);

    ModuleIDAndFolderList getAllModules() const;
    ModuleIDAndFolder getModuleWithID (const String&) const;

    void removeDuplicates (const ModuleIDAndFolderList& other);

    //==============================================================================
    struct Listener
    {
        virtual ~Listener() {}

        virtual void availableModulesChanged() = 0;
    };

    void addListener (Listener* listenerToAdd)          { listeners.add (listenerToAdd); }
    void removeListener (Listener* listenerToRemove)    { listeners.remove (listenerToRemove); }

private:
    ThreadPoolJob* createScannerJob (const Array<File>&);
    void removePendingAndAddJob (ThreadPoolJob*);

    ThreadPool scanPool { 1 };

    ModuleIDAndFolderList moduleList;
    ListenerList<Listener> listeners;
    CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AvailableModuleList)
};

//==============================================================================
class EnabledModuleList
{
public:
    EnabledModuleList (Project&, const ValueTree&);

    //==============================================================================
    ValueTree getState() const              { return state; }

    StringArray getAllModules() const;
    void createRequiredModules (OwnedArray<LibraryModule>& modules);
    void sortAlphabetically();

    File getDefaultModulesFolder() const;

    int getNumModules() const               { return state.getNumChildren(); }
    String getModuleID (int index) const    { return state.getChild (index) [Ids::ID].toString(); }

    ModuleDescription getModuleInfo (const String& moduleID);

    bool isModuleEnabled (const String& moduleID) const;
    StringArray getExtraDependenciesNeeded (const String& moduleID) const;
    bool doesModuleHaveHigherCppStandardThanProject (const String& moduleID);

    bool shouldUseGlobalPath (const String& moduleID) const;
    Value shouldUseGlobalPathValue (const String& moduleID) const;

    bool shouldShowAllModuleFilesInProject (const String& moduleID) const;
    Value shouldShowAllModuleFilesInProjectValue (const String& moduleID) const;

    bool shouldCopyModuleFilesLocally (const String& moduleID) const;
    Value shouldCopyModuleFilesLocallyValue (const String& moduleID) const;

    bool areMostModulesUsingGlobalPath() const;
    bool areMostModulesCopiedLocally() const;

    //==============================================================================
    void addModule (const File& moduleManifestFile, bool copyLocally, bool useGlobalPath, bool sendAnalyticsEvent);
    void addModuleInteractive (const String& moduleID);
    void addModuleFromUserSelectedFile();
    void addModuleOfferingToCopy (const File&, bool isFromUserSpecifiedFolder);

    void removeModule (String moduleID);

private:
    UndoManager* getUndoManager() const     { return project.getUndoManagerFor (state); }

    Project& project;
    ValueTree state;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnabledModuleList)
};
