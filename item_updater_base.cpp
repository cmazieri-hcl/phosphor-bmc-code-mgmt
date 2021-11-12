
// called inside void ItemUpdater::createActivation(sdbusplus::message::message& msg)
struct ActivationMessage
{
    ActivationMessage(sdbusplus::message::message& msg);
    ActivationMessage() = delete;
    bool isPurposeBMC();
    bool isPurposeHOST();
    bool isPurposeSYSTEM();
    bool isPurposeUNKNOWN();
    bool isValid();

    SVersion::VersionPurpose = SVersion::VersionPurpose::Unknown;
    std::string version;
    std::string extendedVersion;
    std::string path(std::move(objPath));
    std::string filePath;
    std::string versionId;

 private:
    void readMessage(sdbusplus::message::message& msg);
};
