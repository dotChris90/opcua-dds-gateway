#include "gateway/dds_to_opcua/AddressSpaceBuilder.hpp"
#include <iostream>

namespace gateway {
namespace dds_to_opcua {

AddressSpaceBuilder::AddressSpaceBuilder(UA_Server* server) : server_(server) {}

AddressSpaceBuilder::~AddressSpaceBuilder() {}

UA_NodeId AddressSpaceBuilder::createDomainNode(int32_t domainId) {
    std::string domainName = "Domain_" + std::to_string(domainId);
    std::string nodeIdStr = domainName;

    UA_NodeId domainNodeId = UA_NODEID_STRING_ALLOC(1, nodeIdStr.c_str());

    // Check if domain already exists
    UA_NodeId existingCheck;
    if (UA_Server_readNodeId(server_, domainNodeId, &existingCheck) == UA_STATUSCODE_GOOD) {
        UA_NodeId_clear(&existingCheck);
        return domainNodeId;
    }

    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    oAttr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", domainName.c_str());
    oAttr.description = UA_LOCALIZEDTEXT_ALLOC("en-US", "DDS Global Data Space Domain");

    UA_Server_addObjectNode(server_,
                            domainNodeId,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                            UA_QUALIFIEDNAME_ALLOC(1, domainName.c_str()),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                            oAttr, nullptr, nullptr);

    UA_LocalizedText_clear(&oAttr.displayName);
    UA_LocalizedText_clear(&oAttr.description);

    // Add mandatory DomainId property (§9.3.2.1 Table 9.45)
    std::string propIdStr = domainName + ".DomainId";
    UA_VariableAttributes vAttr = UA_VariableAttributes_default;
    vAttr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", "DomainId");
    vAttr.accessLevel = UA_ACCESSLEVELMASK_READ;
    vAttr.writeMask = 0;
    vAttr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
    UA_Int32 uaDomainId = domainId;
    UA_Variant_setScalarCopy(&vAttr.value, &uaDomainId, &UA_TYPES[UA_TYPES_INT32]);

    UA_Server_addVariableNode(server_,
                              UA_NODEID_STRING_ALLOC(1, propIdStr.c_str()),
                              domainNodeId,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                              UA_QUALIFIEDNAME_ALLOC(1, "DomainId"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_PROPERTYTYPE),
                              vAttr, nullptr, nullptr);

    UA_LocalizedText_clear(&vAttr.displayName);
    UA_Variant_clear(&vAttr.value);

    return domainNodeId;
}

UA_NodeId AddressSpaceBuilder::createTopicNode(const UA_NodeId& domainNodeId,
                                               const std::string& topicName,
                                               const std::string& registeredTypeName,
                                               bool isKeyed) {
    (void)isKeyed;
    std::string nodeIdStr = topicName;
    UA_NodeId topicNodeId = UA_NODEID_STRING_ALLOC(1, nodeIdStr.c_str());

    // Check if already created
    UA_NodeId existingCheck;
    if (UA_Server_readNodeId(server_, topicNodeId, &existingCheck) == UA_STATUSCODE_GOOD) {
        UA_NodeId_clear(&existingCheck);
        return topicNodeId;
    }

    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    oAttr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", topicName.c_str());
    oAttr.description = UA_LOCALIZEDTEXT_ALLOC("en-US", ("DDS Topic: " + topicName).c_str());

    UA_Server_addObjectNode(server_,
                            topicNodeId,
                            domainNodeId,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                            UA_QUALIFIEDNAME_ALLOC(1, topicName.c_str()),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                            oAttr, nullptr, nullptr);

    UA_LocalizedText_clear(&oAttr.displayName);
    UA_LocalizedText_clear(&oAttr.description);

    // Add RegisteredTypeName property (§9.3.3.1 Table 9.47)
    std::string propIdStr = topicName + ".RegisteredTypeName";
    UA_VariableAttributes vAttr = UA_VariableAttributes_default;
    vAttr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", "RegisteredTypeName");
    vAttr.accessLevel = UA_ACCESSLEVELMASK_READ;
    vAttr.writeMask = 0;
    vAttr.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
    UA_String uaTypeName = UA_STRING_ALLOC(registeredTypeName.c_str());
    UA_Variant_setScalarCopy(&vAttr.value, &uaTypeName, &UA_TYPES[UA_TYPES_STRING]);
    UA_String_clear(&uaTypeName);

    UA_Server_addVariableNode(server_,
                              UA_NODEID_STRING_ALLOC(1, propIdStr.c_str()),
                              topicNodeId,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                              UA_QUALIFIEDNAME_ALLOC(1, "RegisteredTypeName"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_PROPERTYTYPE),
                              vAttr, nullptr, nullptr);

    UA_LocalizedText_clear(&vAttr.displayName);
    UA_Variant_clear(&vAttr.value);

    return topicNodeId;
}

InstanceNodeInfo AddressSpaceBuilder::createInstanceNode(const UA_NodeId& topicNodeId,
                                                        const std::string& topicName,
                                                        const std::string& instanceId,
                                                        const std::vector<std::pair<std::string, const UA_DataType*>>& fields,
                                                        const std::string& keyFieldName) {
    InstanceNodeInfo info;
    info.instance_id = instanceId;

    std::string browseName = instanceId.empty() ? "Instance" : ("Instance_" + instanceId);
    std::string baseNodeIdStr = topicName + "." + browseName;
    info.instance_node_id = UA_NODEID_STRING_ALLOC(1, baseNodeIdStr.c_str());

    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    oAttr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", browseName.c_str());

    UA_Server_addObjectNode(server_,
                            info.instance_node_id,
                            topicNodeId,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                            UA_QUALIFIEDNAME_ALLOC(1, browseName.c_str()),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                            oAttr, nullptr, nullptr);

    UA_LocalizedText_clear(&oAttr.displayName);

    // Create SampleInfo Properties (§9.3.4.1 Table 9.52)
    auto addSampleProperty = [&](const std::string& propName, const UA_DataType* type, UA_NodeId& outNodeId) {
        std::string propIdStr = baseNodeIdStr + "." + propName;
        outNodeId = UA_NODEID_STRING_ALLOC(1, propIdStr.c_str());

        UA_VariableAttributes pAttr = UA_VariableAttributes_default;
        pAttr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", propName.c_str());
        pAttr.accessLevel = UA_ACCESSLEVELMASK_READ;
        pAttr.writeMask = 0;
        pAttr.dataType = type->typeId;

        UA_Server_addVariableNode(server_,
                                  outNodeId,
                                  info.instance_node_id,
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                  UA_QUALIFIEDNAME_ALLOC(1, propName.c_str()),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_PROPERTYTYPE),
                                  pAttr, nullptr, nullptr);

        UA_LocalizedText_clear(&pAttr.displayName);
    };

    addSampleProperty("SampleState", &UA_TYPES[UA_TYPES_STRING], info.sample_state_node_id);
    addSampleProperty("ViewState", &UA_TYPES[UA_TYPES_STRING], info.view_state_node_id);
    addSampleProperty("InstanceState", &UA_TYPES[UA_TYPES_STRING], info.instance_state_node_id);
    addSampleProperty("SourceTimestamp", &UA_TYPES[UA_TYPES_DATETIME], info.source_timestamp_node_id);
    addSampleProperty("InstanceHandle", &UA_TYPES[UA_TYPES_STRING], info.instance_handle_node_id);
    addSampleProperty("ValidData", &UA_TYPES[UA_TYPES_BOOLEAN], info.valid_data_node_id);

    // Create Data Fields
    for (const auto& field : fields) {
        const std::string& fieldName = field.first;
        const UA_DataType* fieldType = field.second;

        std::string fieldIdStr = baseNodeIdStr + "." + fieldName;
        UA_NodeId fieldNodeId = UA_NODEID_STRING_ALLOC(1, fieldIdStr.c_str());

        UA_VariableAttributes fAttr = UA_VariableAttributes_default;
        fAttr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", fieldName.c_str());
        
        bool isKey = (!keyFieldName.empty() && fieldName == keyFieldName);
        if (isKey) {
            // Key fields are read-only (§9.2.8)
            fAttr.accessLevel = UA_ACCESSLEVELMASK_READ;
            fAttr.writeMask = 0;
        } else {
            // Non-key members can be written by OPC UA Clients (§9.3.4.5)
            fAttr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
        }
        fAttr.dataType = fieldType->typeId;

        // Initialize default zero value based on type
        if (fieldType == &UA_TYPES[UA_TYPES_INT32]) {
            UA_Int32 zero = 0;
            UA_Variant_setScalarCopy(&fAttr.value, &zero, fieldType);
        } else if (fieldType == &UA_TYPES[UA_TYPES_DOUBLE]) {
            UA_Double zero = 0.0;
            UA_Variant_setScalarCopy(&fAttr.value, &zero, fieldType);
        } else if (fieldType == &UA_TYPES[UA_TYPES_BOOLEAN]) {
            UA_Boolean f = false;
            UA_Variant_setScalarCopy(&fAttr.value, &f, fieldType);
        } else if (fieldType == &UA_TYPES[UA_TYPES_STRING]) {
            UA_String emptyStr = UA_STRING_ALLOC(isKey ? instanceId.c_str() : "");
            UA_Variant_setScalarCopy(&fAttr.value, &emptyStr, fieldType);
            UA_String_clear(&emptyStr);
        }

        UA_Server_addVariableNode(server_,
                                  fieldNodeId,
                                  info.instance_node_id,
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                  UA_QUALIFIEDNAME_ALLOC(1, fieldName.c_str()),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                  fAttr, nullptr, nullptr);

        UA_LocalizedText_clear(&fAttr.displayName);
        UA_Variant_clear(&fAttr.value);

        info.field_node_ids[fieldName] = fieldNodeId;
    }

    return info;
}

void AddressSpaceBuilder::updateSampleInfo(const InstanceNodeInfo& info,
                                           const std::string& sampleState,
                                           const std::string& viewState,
                                           const std::string& instanceState,
                                           int64_t sourceTimestampSec,
                                           uint32_t sourceTimestampNano,
                                           const std::string& instanceHandle,
                                           bool validData) {
    updateStringField(info.sample_state_node_id, sampleState);
    updateStringField(info.view_state_node_id, viewState);
    updateStringField(info.instance_state_node_id, instanceState);
    updateStringField(info.instance_handle_node_id, instanceHandle);
    updateBooleanField(info.valid_data_node_id, validData);

    UA_DateTime uaDt = types::ddsTimeToOpcUaDateTime(sourceTimestampSec, sourceTimestampNano);
    UA_Variant var;
    UA_Variant_init(&var);
    UA_Variant_setScalarCopy(&var, &uaDt, &UA_TYPES[UA_TYPES_DATETIME]);
    UA_Server_writeValue(server_, info.source_timestamp_node_id, var);
    UA_Variant_clear(&var);
}

void AddressSpaceBuilder::updateInt32Field(const UA_NodeId& fieldNodeId, int32_t val) {
    UA_Variant var;
    UA_Variant_init(&var);
    UA_Int32 uaVal = val;
    UA_Variant_setScalarCopy(&var, &uaVal, &UA_TYPES[UA_TYPES_INT32]);
    UA_Server_writeValue(server_, fieldNodeId, var);
    UA_Variant_clear(&var);
}

void AddressSpaceBuilder::updateStringField(const UA_NodeId& fieldNodeId, const std::string& val) {
    UA_Variant var;
    UA_Variant_init(&var);
    UA_String uaStr = types::toUaString(val);
    UA_Variant_setScalarCopy(&var, &uaStr, &UA_TYPES[UA_TYPES_STRING]);
    UA_String_clear(&uaStr);
    UA_Server_writeValue(server_, fieldNodeId, var);
    UA_Variant_clear(&var);
}

void AddressSpaceBuilder::updateDoubleField(const UA_NodeId& fieldNodeId, double val) {
    UA_Variant var;
    UA_Variant_init(&var);
    UA_Double uaVal = val;
    UA_Variant_setScalarCopy(&var, &uaVal, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(server_, fieldNodeId, var);
    UA_Variant_clear(&var);
}

void AddressSpaceBuilder::updateBooleanField(const UA_NodeId& fieldNodeId, bool val) {
    UA_Variant var;
    UA_Variant_init(&var);
    UA_Boolean uaVal = val;
    UA_Variant_setScalarCopy(&var, &uaVal, &UA_TYPES[UA_TYPES_BOOLEAN]);
    UA_Server_writeValue(server_, fieldNodeId, var);
    UA_Variant_clear(&var);
}

} // namespace dds_to_opcua
} // namespace gateway
