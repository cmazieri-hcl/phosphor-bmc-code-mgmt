#pragma once

#include "activation.hpp"
#include "item_updater_helper.hpp"
#include "version.hpp"
#include "xyz/openbmc_project/Collection/DeleteAll/server.hpp"

#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Common/FactoryReset/server.hpp>
#include <xyz/openbmc_project/Control/FieldMode/server.hpp>

#include <string>
#include <vector>

namespace phosphor
{
namespace software
{
namespace updater
{

// When you see server:: you know we're referencing our base class
namespace server = sdbusplus::xyz::openbmc_project::Software::server;
namespace control = sdbusplus::xyz::openbmc_project::Control::server;

using ItemUpdaterInherit = sdbusplus::server::object::object<
    sdbusplus::xyz::openbmc_project::Common::server::FactoryReset,
    sdbusplus::xyz::openbmc_project::Control::server::FieldMode,
    sdbusplus::xyz::openbmc_project::Association::server::Definitions,
    sdbusplus::xyz::openbmc_project::Collection::server::DeleteAll>;

namespace MatchRules = sdbusplus::bus::match::rules;
using VersionClass = phosphor::software::manager::Version;
using AssociationList =
    std::vector<std::tuple<std::string, std::string, std::string>>;

using VersionPurpose = server::Version::VersionPurpose;

// used in ItemUpdater::createActivation(sdbusplus::message::message& msg)
// implementation in item_updater_base.cpp
struct SoftwareVersionMessage
{
    SoftwareVersionMessage(sdbusplus::message::message& msg);
    SoftwareVersionMessage() = delete;
    bool isPurposeBMC() const;
    bool isPurposeHOST() const;
    bool isPurposeSYSTEM() const;
    bool isPurposeUnknown() const;
    /** @brief returns true if
     *         purpose != VersionPurpose::Unknown
     *     and version   is not empty
     *     and path      is not empty
     *     and filePath  is not empty
     *     and versionId is not empty
     **/
    bool isValid() const;
    VersionPurpose  purpose = VersionPurpose::Unknown;
    std::string version;
    std::string extendedVersion;
    std::string path;
    std::string filePath;
    std::string versionId;
 private:
    void readMessage(sdbusplus::message::message& msg);
};


/** @class ItemUpdater
 *  @brief Manages the activation of the BMC version items.
 */
class ItemUpdater : public ItemUpdaterInherit
{
  public:
    /**
     * @brief Types of Activation status for image validation.
     */
    enum class ActivationStatus
    {
        ready,
        invalid,
        active
    };

    /** @brief Constructs ItemUpdater
     *
     * @param[in] bus    - The D-Bus bus object
     */
    ItemUpdater(sdbusplus::bus::bus& bus, const std::string& path) :
        ItemUpdaterInherit(bus, path.c_str(), false),
        bus(bus), helper(bus), img_obj_path(path),
        versionMatch(bus,
                     MatchRules::interfacesAdded() +
                         MatchRules::path("/xyz/openbmc_project/software"),
                     std::bind(std::mem_fn(&ItemUpdater::createActivation),
                               this, std::placeholders::_1))
    {
        emit_object_added();
    }

    void reset() override;

    /** @brief Save priority value to persistent storage (flash and optionally
     *  a U-Boot environment variable)
     *
     *  @param[in] versionId - The Id of the version
     *  @param[in] value - The priority value
     *  @return None
     */
    void savePriority(const std::string& versionId, uint8_t value);

    /**
     * @brief Pure virtual function to finish an update
     * @param imageVersionId the versionId
     */
    virtual void onActivationDone(const std::string& imageVersionId) = 0;


    /** @brief Sets the given priority free by incrementing
     *  any existing priority with the same value by 1
     *
     *  @param[in] value - The priority that needs to be set free.
     *  @param[in] versionId - The Id of the version for which we
     *                         are trying to free up the priority.
     *  @return None
     */
    virtual void freePriority(uint8_t value, const std::string& versionId) = 0;

    /**
     * @brief Erase specified entry D-Bus object
     *        if Action property is not set to Active
     *
     * @param[in] entryId - unique identifier of the entry
     */
    virtual void erase(std::string entryId) = 0;

    /**
     * @brief Deletes all versions except for the current one
     */
     void deleteAll() override;

     /**
      * @brief Deletes the version from Image Manager and the
      *        untar image from image upload dir.
      */
     void deleteImageManagerObject();

    /** @brief Creates an active association to the
     *  newly active software image
     *
     * @param[in]  path - The path to create the association to.
     */
    void createActiveAssociation(const std::string& path);

    /** @brief Removes the associations from the provided software image path
     *
     * @param[in]  path - The path to remove the associations from.
     */
    void removeAssociations(const std::string& path);


    /** @brief Brings the total number of active BMC versions to
     *         ACTIVE_BMC_MAX_ALLOWED -1. This function is intended to be
     *         run before activating a new BMC version. If this function
     *         needs to delete any BMC version(s) it will delete the
     *         version(s) with the highest priority, skipping the
     *         functional BMC version.
     *
     * @param[in] caller - The Activation object that called this function.
     */
    virtual void freeSpace(Activation& caller) = 0;

    /** @brief Creates a updateable association to the
     *  "running" BMC software image
     *
     * @param[in]  path - The path to create the association.
     */
    void createUpdateableAssociation(const std::string& path);

    /** @brief Persistent map of Version D-Bus objects and their
     * version id */
    std::map<std::string, std::unique_ptr<VersionClass>> versions;

    /** @brief Vector of needed BMC images in the tarball*/
    std::vector<std::string> imageUpdateList;

  protected:
    /**
     * @brief Helper function to create VersionClass object and Delete object
     * @param versionInfo contains all the information about the version
     */
    void  createVersion(const SoftwareVersionMessage& versionInfo);

    std::string freePriority(ActivationMap& activationContaier, uint8_t value,
                             const std::string& versionId);

    /** @brief Callback function for Software.Version match.
     *  @details Creates an Activation D-Bus object.
     *
     * @param[in]  msg       - Data associated with subscribed signal
     */
    virtual void createActivation(sdbusplus::message::message& msg) = 0;

    /**
     * @brief Validates the presence of SquashFS image in the image dir.
     *
     * @param[in]  filePath  - The path to the image dir.
     * @param[out] result    - ActivationStatus Enum.
     *                         ready if validation was successful.
     *                         invalid if validation fail.
     *                         active if image is the current version.
     *
     */
    ActivationStatus validateSquashFSImage(const std::string& filePath);

    /** @brief Creates a functional association to the
     *  "running" BMC software image
     *
     * @param[in]  path - The path to create the association to.
     */
    void createFunctionalAssociation(const std::string& path);

    /** @brief Persistent sdbusplus D-Bus bus connection. */
    sdbusplus::bus::bus& bus;

    /** @brief The helper of image updater. */
    Helper helper;

    /** @brief object path of the image  */
    std::string img_obj_path;

    /** @brief sdbusplus signal match for Software.Version */
    sdbusplus::bus::match_t versionMatch;

    /** @brief This entry's associations */
    AssociationList assocs = {};

    /** @brief Clears read only partition for
     * given Activation D-Bus object.
     *
     * @param[in]  versionId - The version id.
     */
    void removeReadOnlyPartition(std::string versionId);

    /** @brief Check the required image files
     *
     * @param[in] filePath - BMC tarball file path
     * @param[in] imageList - Image filenames included in the BMC tarball
     * @param[out] result - Boolean
     *                      true if all image files are found in BMC tarball
     *                      false if one of image files is missing
     */
    bool checkImage(const std::string& filePath,
                    const std::vector<std::string>& imageList);
};

} // namespace updater
} // namespace software
} // namespace phosphor
