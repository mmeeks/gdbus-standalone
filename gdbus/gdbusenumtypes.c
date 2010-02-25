
/* Generated data (by glib-mkenums) */

#include <gdbus.h>

/* enumerations from "gvariant.h" */
GType
struct_struct_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GEnumValue values[] = {
        { G_VARIANT_CLASS_BOOLEAN, "G_VARIANT_CLASS_BOOLEAN", "boolean" },
        { G_VARIANT_CLASS_BYTE, "G_VARIANT_CLASS_BYTE", "byte" },
        { G_VARIANT_CLASS_INT16, "G_VARIANT_CLASS_INT16", "int16" },
        { G_VARIANT_CLASS_UINT16, "G_VARIANT_CLASS_UINT16", "uint16" },
        { G_VARIANT_CLASS_INT32, "G_VARIANT_CLASS_INT32", "int32" },
        { G_VARIANT_CLASS_UINT32, "G_VARIANT_CLASS_UINT32", "uint32" },
        { G_VARIANT_CLASS_INT64, "G_VARIANT_CLASS_INT64", "int64" },
        { G_VARIANT_CLASS_UINT64, "G_VARIANT_CLASS_UINT64", "uint64" },
        { G_VARIANT_CLASS_HANDLE, "G_VARIANT_CLASS_HANDLE", "handle" },
        { G_VARIANT_CLASS_DOUBLE, "G_VARIANT_CLASS_DOUBLE", "double" },
        { G_VARIANT_CLASS_STRING, "G_VARIANT_CLASS_STRING", "string" },
        { G_VARIANT_CLASS_OBJECT_PATH, "G_VARIANT_CLASS_OBJECT_PATH", "object-path" },
        { G_VARIANT_CLASS_SIGNATURE, "G_VARIANT_CLASS_SIGNATURE", "signature" },
        { G_VARIANT_CLASS_VARIANT, "G_VARIANT_CLASS_VARIANT", "variant" },
        { G_VARIANT_CLASS_MAYBE, "G_VARIANT_CLASS_MAYBE", "maybe" },
        { G_VARIANT_CLASS_ARRAY, "G_VARIANT_CLASS_ARRAY", "array" },
        { G_VARIANT_CLASS_TUPLE, "G_VARIANT_CLASS_TUPLE", "tuple" },
        { G_VARIANT_CLASS_DICT_ENTRY, "G_VARIANT_CLASS_DICT_ENTRY", "dict-entry" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("struct"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

GType
g_variant_builder_error_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GEnumValue values[] = {
        { G_VARIANT_BUILDER_ERROR_TOO_MANY, "G_VARIANT_BUILDER_ERROR_TOO_MANY", "too-many" },
        { G_VARIANT_BUILDER_ERROR_TOO_FEW, "G_VARIANT_BUILDER_ERROR_TOO_FEW", "too-few" },
        { G_VARIANT_BUILDER_ERROR_INFER, "G_VARIANT_BUILDER_ERROR_INFER", "infer" },
        { G_VARIANT_BUILDER_ERROR_TYPE, "G_VARIANT_BUILDER_ERROR_TYPE", "type" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("GVariantBuilderError"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

GType
g_variant_flags_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GEnumValue values[] = {
        { G_VARIANT_LITTLE_ENDIAN, "G_VARIANT_LITTLE_ENDIAN", "little-endian" },
        { G_VARIANT_BIG_ENDIAN, "G_VARIANT_BIG_ENDIAN", "big-endian" },
        { G_VARIANT_TRUSTED, "G_VARIANT_TRUSTED", "trusted" },
        { G_VARIANT_LAZY_BYTESWAP, "G_VARIANT_LAZY_BYTESWAP", "lazy-byteswap" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("GVariantFlags"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

/* enumerations from "gdbusenums.h" */
GType
g_bus_type_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GEnumValue values[] = {
        { G_BUS_TYPE_NONE, "G_BUS_TYPE_NONE", "none" },
        { G_BUS_TYPE_SESSION, "G_BUS_TYPE_SESSION", "session" },
        { G_BUS_TYPE_SYSTEM, "G_BUS_TYPE_SYSTEM", "system" },
        { G_BUS_TYPE_STARTER, "G_BUS_TYPE_STARTER", "starter" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("GBusType"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

GType
g_bus_name_owner_flags_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GFlagsValue values[] = {
        { G_BUS_NAME_OWNER_FLAGS_NONE, "G_BUS_NAME_OWNER_FLAGS_NONE", "none" },
        { G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT, "G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT", "allow-replacement" },
        { G_BUS_NAME_OWNER_FLAGS_REPLACE, "G_BUS_NAME_OWNER_FLAGS_REPLACE", "replace" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_flags_register_static (g_intern_static_string ("GBusNameOwnerFlags"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

GType
g_dbus_proxy_flags_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GFlagsValue values[] = {
        { G_DBUS_PROXY_FLAGS_NONE, "G_DBUS_PROXY_FLAGS_NONE", "none" },
        { G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, "G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES", "do-not-load-properties" },
        { G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS, "G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS", "do-not-connect-signals" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_flags_register_static (g_intern_static_string ("GDBusProxyFlags"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

GType
g_dbus_error_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GEnumValue values[] = {
        { G_DBUS_ERROR_FAILED, "G_DBUS_ERROR_FAILED", "failed" },
        { G_DBUS_ERROR_CANCELLED, "G_DBUS_ERROR_CANCELLED", "cancelled" },
        { G_DBUS_ERROR_CONVERSION_FAILED, "G_DBUS_ERROR_CONVERSION_FAILED", "conversion-failed" },
        { G_DBUS_ERROR_REMOTE_ERROR, "G_DBUS_ERROR_REMOTE_ERROR", "remote-error" },
        { G_DBUS_ERROR_DBUS_FAILED, "G_DBUS_ERROR_DBUS_FAILED", "dbus-failed" },
        { G_DBUS_ERROR_NO_MEMORY, "G_DBUS_ERROR_NO_MEMORY", "no-memory" },
        { G_DBUS_ERROR_SERVICE_UNKNOWN, "G_DBUS_ERROR_SERVICE_UNKNOWN", "service-unknown" },
        { G_DBUS_ERROR_NAME_HAS_NO_OWNER, "G_DBUS_ERROR_NAME_HAS_NO_OWNER", "name-has-no-owner" },
        { G_DBUS_ERROR_NO_REPLY, "G_DBUS_ERROR_NO_REPLY", "no-reply" },
        { G_DBUS_ERROR_IO_ERROR, "G_DBUS_ERROR_IO_ERROR", "io-error" },
        { G_DBUS_ERROR_BAD_ADDRESS, "G_DBUS_ERROR_BAD_ADDRESS", "bad-address" },
        { G_DBUS_ERROR_NOT_SUPPORTED, "G_DBUS_ERROR_NOT_SUPPORTED", "not-supported" },
        { G_DBUS_ERROR_LIMITS_EXCEEDED, "G_DBUS_ERROR_LIMITS_EXCEEDED", "limits-exceeded" },
        { G_DBUS_ERROR_ACCESS_DENIED, "G_DBUS_ERROR_ACCESS_DENIED", "access-denied" },
        { G_DBUS_ERROR_AUTH_FAILED, "G_DBUS_ERROR_AUTH_FAILED", "auth-failed" },
        { G_DBUS_ERROR_NO_SERVER, "G_DBUS_ERROR_NO_SERVER", "no-server" },
        { G_DBUS_ERROR_TIMEOUT, "G_DBUS_ERROR_TIMEOUT", "timeout" },
        { G_DBUS_ERROR_NO_NETWORK, "G_DBUS_ERROR_NO_NETWORK", "no-network" },
        { G_DBUS_ERROR_ADDRESS_IN_USE, "G_DBUS_ERROR_ADDRESS_IN_USE", "address-in-use" },
        { G_DBUS_ERROR_DISCONNECTED, "G_DBUS_ERROR_DISCONNECTED", "disconnected" },
        { G_DBUS_ERROR_INVALID_ARGS, "G_DBUS_ERROR_INVALID_ARGS", "invalid-args" },
        { G_DBUS_ERROR_FILE_NOT_FOUND, "G_DBUS_ERROR_FILE_NOT_FOUND", "file-not-found" },
        { G_DBUS_ERROR_FILE_EXISTS, "G_DBUS_ERROR_FILE_EXISTS", "file-exists" },
        { G_DBUS_ERROR_UNKNOWN_METHOD, "G_DBUS_ERROR_UNKNOWN_METHOD", "unknown-method" },
        { G_DBUS_ERROR_TIMED_OUT, "G_DBUS_ERROR_TIMED_OUT", "timed-out" },
        { G_DBUS_ERROR_MATCH_RULE_NOT_FOUND, "G_DBUS_ERROR_MATCH_RULE_NOT_FOUND", "match-rule-not-found" },
        { G_DBUS_ERROR_MATCH_RULE_INVALID, "G_DBUS_ERROR_MATCH_RULE_INVALID", "match-rule-invalid" },
        { G_DBUS_ERROR_SPAWN_EXEC_FAILED, "G_DBUS_ERROR_SPAWN_EXEC_FAILED", "spawn-exec-failed" },
        { G_DBUS_ERROR_SPAWN_FORK_FAILED, "G_DBUS_ERROR_SPAWN_FORK_FAILED", "spawn-fork-failed" },
        { G_DBUS_ERROR_SPAWN_CHILD_EXITED, "G_DBUS_ERROR_SPAWN_CHILD_EXITED", "spawn-child-exited" },
        { G_DBUS_ERROR_SPAWN_CHILD_SIGNALED, "G_DBUS_ERROR_SPAWN_CHILD_SIGNALED", "spawn-child-signaled" },
        { G_DBUS_ERROR_SPAWN_FAILED, "G_DBUS_ERROR_SPAWN_FAILED", "spawn-failed" },
        { G_DBUS_ERROR_SPAWN_SETUP_FAILED, "G_DBUS_ERROR_SPAWN_SETUP_FAILED", "spawn-setup-failed" },
        { G_DBUS_ERROR_SPAWN_CONFIG_INVALID, "G_DBUS_ERROR_SPAWN_CONFIG_INVALID", "spawn-config-invalid" },
        { G_DBUS_ERROR_SPAWN_SERVICE_INVALID, "G_DBUS_ERROR_SPAWN_SERVICE_INVALID", "spawn-service-invalid" },
        { G_DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND, "G_DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND", "spawn-service-not-found" },
        { G_DBUS_ERROR_SPAWN_PERMISSIONS_INVALID, "G_DBUS_ERROR_SPAWN_PERMISSIONS_INVALID", "spawn-permissions-invalid" },
        { G_DBUS_ERROR_SPAWN_FILE_INVALID, "G_DBUS_ERROR_SPAWN_FILE_INVALID", "spawn-file-invalid" },
        { G_DBUS_ERROR_SPAWN_NO_MEMORY, "G_DBUS_ERROR_SPAWN_NO_MEMORY", "spawn-no-memory" },
        { G_DBUS_ERROR_UNIX_PROCESS_ID_UNKNOWN, "G_DBUS_ERROR_UNIX_PROCESS_ID_UNKNOWN", "unix-process-id-unknown" },
        { G_DBUS_ERROR_INVALID_SIGNATURE, "G_DBUS_ERROR_INVALID_SIGNATURE", "invalid-signature" },
        { G_DBUS_ERROR_INVALID_FILE_CONTENT, "G_DBUS_ERROR_INVALID_FILE_CONTENT", "invalid-file-content" },
        { G_DBUS_ERROR_SELINUX_SECURITY_CONTEXT_UNKNOWN, "G_DBUS_ERROR_SELINUX_SECURITY_CONTEXT_UNKNOWN", "selinux-security-context-unknown" },
        { G_DBUS_ERROR_ADT_AUDIT_DATA_UNKNOWN, "G_DBUS_ERROR_ADT_AUDIT_DATA_UNKNOWN", "adt-audit-data-unknown" },
        { G_DBUS_ERROR_OBJECT_PATH_IN_USE, "G_DBUS_ERROR_OBJECT_PATH_IN_USE", "object-path-in-use" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("GDBusError"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

GType
g_dbus_property_info_flags_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GFlagsValue values[] = {
        { G_DBUS_PROPERTY_INFO_FLAGS_NONE, "G_DBUS_PROPERTY_INFO_FLAGS_NONE", "none" },
        { G_DBUS_PROPERTY_INFO_FLAGS_READABLE, "G_DBUS_PROPERTY_INFO_FLAGS_READABLE", "readable" },
        { G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE, "G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE", "writable" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_flags_register_static (g_intern_static_string ("GDBusPropertyInfoFlags"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

GType
g_dbus_subtree_flags_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GFlagsValue values[] = {
        { G_DBUS_SUBTREE_FLAGS_NONE, "G_DBUS_SUBTREE_FLAGS_NONE", "none" },
        { G_DBUS_SUBTREE_FLAGS_DISPATCH_TO_UNENUMERATED_NODES, "G_DBUS_SUBTREE_FLAGS_DISPATCH_TO_UNENUMERATED_NODES", "dispatch-to-unenumerated-nodes" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_flags_register_static (g_intern_static_string ("GDBusSubtreeFlags"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}


/* Generated data ends here */

