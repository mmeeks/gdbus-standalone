/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2009 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#if !defined (__G_DBUS_G_DBUS_H_INSIDE__) && !defined (G_DBUS_COMPILATION)
#error "Only <gdbus/gdbus.h> can be included directly."
#endif

#ifndef __G_DBUS_ENUMS_H__
#define __G_DBUS_ENUMS_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * GBusType:
 * @G_BUS_TYPE_NONE: Not a message bus connection.
 * @G_BUS_TYPE_SESSION: The login session message bus.
 * @G_BUS_TYPE_SYSTEM: The system-wide message bus.
 * @G_BUS_TYPE_STARTER: Connect to the bus that activated the program.
 *
 * An enumeration to specify the type of a #GDBusConnection.
 */
typedef enum
{
  G_BUS_TYPE_NONE    = -1,
  G_BUS_TYPE_SESSION = 0,
  G_BUS_TYPE_SYSTEM  = 1,
  G_BUS_TYPE_STARTER = 2
} GBusType;

/**
 * GBusNameOwnerFlags:
 * @G_BUS_NAME_OWNER_FLAGS_NONE: No flags set.
 * @G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT: Allow another message connection to take the name.
 * @G_BUS_NAME_OWNER_FLAGS_REPLACE: If another message bus connection
 * owns the name and have specified #G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT, then
 * take the name from the other connection.
 *
 * Flags used when constructing a #GBusNameOwner.
 */
typedef enum
{
  G_BUS_NAME_OWNER_FLAGS_NONE = 0,                    /*< nick=none >*/
  G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT = (1<<0),  /*< nick=allow-replacement >*/
  G_BUS_NAME_OWNER_FLAGS_REPLACE = (1<<1),            /*< nick=replace >*/
} GBusNameOwnerFlags;

/**
 * GDBusProxyFlags:
 * @G_DBUS_PROXY_FLAGS_NONE: No flags set.
 * @G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES: Don't load properties.
 * @G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS: Don't connect to signals on the remote object.
 *
 * Flags used when constructing an instance of a #GDBusProxy derived class.
 */
typedef enum
{
  G_DBUS_PROXY_FLAGS_NONE = 0,                        /*< nick=none >*/
  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES = (1<<0), /*< nick=do-not-load-properties >*/
  G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS = (1<<1), /*< nick=do-not-connect-signals >*/
} GDBusProxyFlags;

/**
 * GDBusError:
 * @G_DBUS_ERROR_FAILED: The operation failed.
 * @G_DBUS_ERROR_CANCELLED: The operation was cancelled.
 * @G_DBUS_ERROR_CONVERSION_FAILED: An attempt to send #GVariant value not compatible with the D-Bus protocol was made.
 * @G_DBUS_ERROR_REMOTE_EXCEPTION: A remote exception that couldn't be
 * mapped to a #GError. Use g_dbus_error_get_remote_exception()
 * to extract the D-Bus error name.
 * @G_DBUS_ERROR_DBUS_FAILED:
 * A generic error; "something went wrong" - see the error message for
 * more.
 * @G_DBUS_ERROR_NO_MEMORY:
 * There was not enough memory to complete an operation.
 * @G_DBUS_ERROR_SERVICE_UNKNOWN:
 * The bus doesn't know how to launch a service to supply the bus name
 * you wanted.
 * @G_DBUS_ERROR_NAME_HAS_NO_OWNER:
 * The bus name you referenced doesn't exist (i.e. no application owns
 * it).
 * @G_DBUS_ERROR_NO_REPLY:
 * No reply to a message expecting one, usually means a timeout occurred.
 * @G_DBUS_ERROR_IO_ERROR:
 * Something went wrong reading or writing to a socket, for example.
 * @G_DBUS_ERROR_BAD_ADDRESS:
 * A D-Bus bus address was malformed.
 * @G_DBUS_ERROR_NOT_SUPPORTED:
 * Requested operation isn't supported (like ENOSYS on UNIX).
 * @G_DBUS_ERROR_LIMITS_EXCEEDED:
 * Some limited resource is exhausted.
 * @G_DBUS_ERROR_ACCESS_DENIED:
 * Security restrictions don't allow doing what you're trying to do.
 * @G_DBUS_ERROR_AUTH_FAILED:
 * Authentication didn't work.
 * @G_DBUS_ERROR_NO_SERVER:
 * Unable to connect to server (probably caused by ECONNREFUSED on a
 * socket).
 * @G_DBUS_ERROR_TIMEOUT:
 * Certain timeout errors, possibly ETIMEDOUT on a socket.  Note that
 * #G_DBUS_ERROR_NO_REPLY is used for message reply timeouts. Warning:
 * this is confusingly-named given that #G_DBUS_ERROR_TIMED_OUT also
 * exists. We can't fix it for compatibility reasons so just be
 * careful.
 * @G_DBUS_ERROR_NO_NETWORK:
 * No network access (probably ENETUNREACH on a socket).
 * @G_DBUS_ERROR_ADDRESS_IN_USE:
 * Can't bind a socket since its address is in use (i.e. EADDRINUSE).
 * @G_DBUS_ERROR_DISCONNECTED:
 * The connection is disconnected and you're trying to use it.
 * @G_DBUS_ERROR_INVALID_ARGS:
 * Invalid arguments passed to a method call.
 * @G_DBUS_ERROR_FILE_NOT_FOUND:
 * Missing file.
 * @G_DBUS_ERROR_FILE_EXISTS:
 * Existing file and the operation you're using does not silently overwrite.
 * @G_DBUS_ERROR_UNKNOWN_METHOD:
 * Method name you invoked isn't known by the object you invoked it on.
 * @G_DBUS_ERROR_TIMED_OUT:
 * Certain timeout errors, e.g. while starting a service. Warning: this is
 * confusingly-named given that #G_DBUS_ERROR_TIMEOUT also exists. We
 * can't fix it for compatibility reasons so just be careful.
 * @G_DBUS_ERROR_MATCH_RULE_NOT_FOUND:
 * Tried to remove or modify a match rule that didn't exist.
 * @G_DBUS_ERROR_MATCH_RULE_INVALID:
 * The match rule isn't syntactically valid.
 * @G_DBUS_ERROR_SPAWN_EXEC_FAILED:
 * While starting a new process, the exec() call failed.
 * @G_DBUS_ERROR_SPAWN_FORK_FAILED:
 * While starting a new process, the fork() call failed.
 * @G_DBUS_ERROR_SPAWN_CHILD_EXITED:
 * While starting a new process, the child exited with a status code.
 * @G_DBUS_ERROR_SPAWN_CHILD_SIGNALED:
 * While starting a new process, the child exited on a signal.
 * @G_DBUS_ERROR_SPAWN_FAILED:
 * While starting a new process, something went wrong.
 * @G_DBUS_ERROR_SPAWN_SETUP_FAILED:
 * We failed to setup the environment correctly.
 * @G_DBUS_ERROR_SPAWN_CONFIG_INVALID:
 * We failed to setup the config parser correctly.
 * @G_DBUS_ERROR_SPAWN_SERVICE_INVALID:
 * Bus name was not valid.
 * @G_DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND:
 * Service file not found in system-services directory.
 * @G_DBUS_ERROR_SPAWN_PERMISSIONS_INVALID:
 * Permissions are incorrect on the setuid helper.
 * @G_DBUS_ERROR_SPAWN_FILE_INVALID:
 * Service file invalid (Name, User or Exec missing).
 * @G_DBUS_ERROR_SPAWN_NO_MEMORY:
 * Tried to get a UNIX process ID and it wasn't available.
 * @G_DBUS_ERROR_UNIX_PROCESS_ID_UNKNOWN:
 * Tried to get a UNIX process ID and it wasn't available.
 * @G_DBUS_ERROR_INVALID_SIGNATURE:
 * A type signature is not valid.
 * @G_DBUS_ERROR_INVALID_FILE_CONTENT:
 * A file contains invalid syntax or is otherwise broken.
 * @G_DBUS_ERROR_SELINUX_SECURITY_CONTEXT_UNKNOWN:
 * Asked for SELinux security context and it wasn't available.
 * @G_DBUS_ERROR_ADT_AUDIT_DATA_UNKNOWN:
 * Asked for ADT audit data and it wasn't available.
 * @G_DBUS_ERROR_OBJECT_PATH_IN_USE:
 * There's already an object with the requested object path.
 *
 * Error codes.
 */
typedef enum
{
  G_DBUS_ERROR_FAILED,                           /*< nick=org.gtk.GDBus.Error.Failed >*/
  G_DBUS_ERROR_CANCELLED,                        /*< nick=org.gtk.GDBus.Error.Cancelled >*/
  G_DBUS_ERROR_CONVERSION_FAILED,                /*< nick=org.gtk.GDBus.Error.ConversionFailed >*/
  G_DBUS_ERROR_REMOTE_EXCEPTION,                 /*< nick=org.gtk.GDBus.Error.RemoteException >*/

  /* Well-known errors in the org.freedesktop.DBus.Error namespace */
  G_DBUS_ERROR_DBUS_FAILED            = 1000,    /*< nick=org.freedesktop.DBus.Error.Failed >*/
  G_DBUS_ERROR_NO_MEMORY,                        /*< nick=org.freedesktop.DBus.Error.NoMemory >*/
  G_DBUS_ERROR_SERVICE_UNKNOWN,                  /*< nick=org.freedesktop.DBus.Error.ServiceUnknown >*/
  G_DBUS_ERROR_NAME_HAS_NO_OWNER,                /*< nick=org.freedesktop.DBus.Error.NameHasNoOwner >*/
  G_DBUS_ERROR_NO_REPLY,                         /*< nick=org.freedesktop.DBus.Error.NoReply >*/
  G_DBUS_ERROR_IO_ERROR,                         /*< nick=org.freedesktop.DBus.Error.IOError >*/
  G_DBUS_ERROR_BAD_ADDRESS,                      /*< nick=org.freedesktop.DBus.Error.BadAddress >*/
  G_DBUS_ERROR_NOT_SUPPORTED,                    /*< nick=org.freedesktop.DBus.Error.NotSupported >*/
  G_DBUS_ERROR_LIMITS_EXCEEDED,                  /*< nick=org.freedesktop.DBus.Error.LimitsExceeded >*/
  G_DBUS_ERROR_ACCESS_DENIED,                    /*< nick=org.freedesktop.DBus.Error.AccessDenied >*/
  G_DBUS_ERROR_AUTH_FAILED,                      /*< nick=org.freedesktop.DBus.Error.AuthFailed >*/
  G_DBUS_ERROR_NO_SERVER,                        /*< nick=org.freedesktop.DBus.Error.NoServer >*/
  G_DBUS_ERROR_TIMEOUT,                          /*< nick=org.freedesktop.DBus.Error.Timeout >*/
  G_DBUS_ERROR_NO_NETWORK,                       /*< nick=org.freedesktop.DBus.Error.NoNetwork >*/
  G_DBUS_ERROR_ADDRESS_IN_USE,                   /*< nick=org.freedesktop.DBus.Error.AddressInUse >*/
  G_DBUS_ERROR_DISCONNECTED,                     /*< nick=org.freedesktop.DBus.Error.Disconnected >*/
  G_DBUS_ERROR_INVALID_ARGS,                     /*< nick=org.freedesktop.DBus.Error.InvalidArgs >*/
  G_DBUS_ERROR_FILE_NOT_FOUND,                   /*< nick=org.freedesktop.DBus.Error.FileNotFound >*/
  G_DBUS_ERROR_FILE_EXISTS,                      /*< nick=org.freedesktop.DBus.Error.FileExists >*/
  G_DBUS_ERROR_UNKNOWN_METHOD,                   /*< nick=org.freedesktop.DBus.Error.UnknownMethod >*/
  G_DBUS_ERROR_TIMED_OUT,                        /*< nick=org.freedesktop.DBus.Error.TimedOut >*/
  G_DBUS_ERROR_MATCH_RULE_NOT_FOUND,             /*< nick=org.freedesktop.DBus.Error.MatchRuleNotFound >*/
  G_DBUS_ERROR_MATCH_RULE_INVALID,               /*< nick=org.freedesktop.DBus.Error.MatchRuleInvalid >*/
  G_DBUS_ERROR_SPAWN_EXEC_FAILED,                /*< nick=org.freedesktop.DBus.Error.Spawn.ExecFailed >*/
  G_DBUS_ERROR_SPAWN_FORK_FAILED,                /*< nick=org.freedesktop.DBus.Error.Spawn.ForkFailed >*/
  G_DBUS_ERROR_SPAWN_CHILD_EXITED,               /*< nick=org.freedesktop.DBus.Error.Spawn.ChildExited >*/
  G_DBUS_ERROR_SPAWN_CHILD_SIGNALED,             /*< nick=org.freedesktop.DBus.Error.Spawn.ChildSignaled >*/
  G_DBUS_ERROR_SPAWN_FAILED,                     /*< nick=org.freedesktop.DBus.Error.Spawn.Failed >*/
  G_DBUS_ERROR_SPAWN_SETUP_FAILED,               /*< nick=org.freedesktop.DBus.Error.Spawn.FailedToSetup >*/
  G_DBUS_ERROR_SPAWN_CONFIG_INVALID,             /*< nick=org.freedesktop.DBus.Error.Spawn.ConfigInvalid >*/
  G_DBUS_ERROR_SPAWN_SERVICE_INVALID,            /*< nick=org.freedesktop.DBus.Error.Spawn.ServiceNotValid >*/
  G_DBUS_ERROR_SPAWN_SERVICE_NOT_FOUND,          /*< nick=org.freedesktop.DBus.Error.Spawn.ServiceNotFound >*/
  G_DBUS_ERROR_SPAWN_PERMISSIONS_INVALID,        /*< nick=org.freedesktop.DBus.Error.Spawn.PermissionsInvalid >*/
  G_DBUS_ERROR_SPAWN_FILE_INVALID,               /*< nick=org.freedesktop.DBus.Error.Spawn.FileInvalid >*/
  G_DBUS_ERROR_SPAWN_NO_MEMORY,                  /*< nick=org.freedesktop.DBus.Error.Spawn.NoMemory >*/
  G_DBUS_ERROR_UNIX_PROCESS_ID_UNKNOWN,          /*< nick=org.freedesktop.DBus.Error.UnixProcessIdUnknown >*/
  G_DBUS_ERROR_INVALID_SIGNATURE,                /*< nick=org.freedesktop.DBus.Error.InvalidSignature >*/
  G_DBUS_ERROR_INVALID_FILE_CONTENT,             /*< nick=org.freedesktop.DBus.Error.InvalidFileContent >*/
  G_DBUS_ERROR_SELINUX_SECURITY_CONTEXT_UNKNOWN, /*< nick=org.freedesktop.DBus.Error.SELinuxSecurityContextUnknown >*/
  G_DBUS_ERROR_ADT_AUDIT_DATA_UNKNOWN,           /*< nick=org.freedesktop.DBus.Error.AdtAuditDataUnknown >*/
  G_DBUS_ERROR_OBJECT_PATH_IN_USE,               /*< nick=org.freedesktop.DBus.Error.ObjectPathInUse >*/
} GDBusError;

/**
 * GDBusPropertyInfoFlags:
 * @G_DBUS_PROPERTY_INFO_FLAGS_NONE: No flags set.
 * @G_DBUS_PROPERTY_INFO_FLAGS_READABLE: Property is readable.
 * @G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE: Property is writable.
 *
 * Flags describing the access control of a D-Bus property.
 */
typedef enum
{
  G_DBUS_PROPERTY_INFO_FLAGS_NONE = 0,
  G_DBUS_PROPERTY_INFO_FLAGS_READABLE = (1<<0),
  G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE = (1<<1),
} GDBusPropertyInfoFlags;

G_END_DECLS

#endif /* __G_DBUS_ENUMS_H__ */
