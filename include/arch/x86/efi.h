/*
 * Copyright (c) 2020 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_ARCH_X86_EFI_H
#define ZEPHYR_INCLUDE_ARCH_X86_EFI_H

#ifndef _ASMLANGUAGE

#include <zephyr/types.h>
#include <stdbool.h>

#define __abi __attribute__((ms_abi))

/*
 * This is a quick installment of EFI structures and functions.
 * Only a very minimal subset will be used and documented, thus the
 * lack of documentation at the moment.
 * See the UEFI 2.8b specifications for more information
 * at https://www.uefi.org/specifications
 */

/* Note: all specified attributes/parameters of type char16_t have been
 * translated to uint16_t as, for now, we don't have char16_t and we don't
 * care being pedantic, plus we do not use it yet.
 * This will need to be changed if required.
 */

typedef struct {
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	uint8_t Data4[8];
} efi_guid_t;

struct efi_input_key {
	uint16_t ScanCode;
	uint16_t UnicodeChar;
};

struct efi_table_header {
	uint64_t Signature;
	uint32_t Revision;
	uint32_t HeaderSize;
	uint32_t CRC32;
	uint32_t Reserved;
};

struct efi_simple_text_input;

typedef uintptr_t __abi (*efi_input_reset_t)(struct efi_simple_text_input *This,
					     bool ExtendedVerification);
typedef uintptr_t __abi (*efi_input_read_key_t)(
	struct efi_simple_text_input *This,
	struct efi_input_key *Key);

struct efi_simple_text_input {
	efi_input_reset_t Reset;
	efi_input_read_key_t ReadKeyStroke;
	void *WaitForKey;
};

struct efi_simple_text_output_mode {
	int32_t MaxMode;
	int32_t Mode;
	int32_t Attribute;
	int32_t CursorColumn;
	int32_t CursorRow;
	bool CursorVisible;
};

struct efi_simple_text_output;

typedef uintptr_t __abi (*efi_text_reset_t)(struct efi_simple_text_output *This,
					    bool ExtendedVerification);

typedef uintptr_t __abi (*efi_text_string_t)(
	struct efi_simple_text_output *This,
	uint16_t *String);

typedef uintptr_t __abi (*efi_text_test_string_t)(
	struct efi_simple_text_output *This,
	uint16_t *String);

typedef uintptr_t __abi (*efi_text_query_mode_t)(
	struct efi_simple_text_output *This,
	uintptr_t ModeNumber,
	uintptr_t *Columns,
	uintptr_t *Rows);

typedef uintptr_t __abi (*efi_text_set_mode_t)(
	struct efi_simple_text_output *This,
	uintptr_t ModeNumber);

typedef uintptr_t __abi (*efi_text_set_attribute_t)(
	struct efi_simple_text_output *This,
	uintptr_t Attribute);

typedef uintptr_t __abi (*efi_text_clear_screen_t)(
	struct efi_simple_text_output *This);

typedef uintptr_t __abi (*efi_text_cursor_position_t)(
	struct efi_simple_text_output *This,
	uintptr_t Column,
	uintptr_t Row);

typedef uintptr_t __abi (*efi_text_enable_cursor_t)(
	struct efi_simple_text_output *This,
	bool Visible);

struct efi_simple_text_output {
	efi_text_reset_t Reset;
	efi_text_string_t OutputString;
	efi_text_test_string_t TestString;
	efi_text_query_mode_t QueryMode;
	efi_text_set_mode_t SetMode;
	efi_text_set_attribute_t SetAttribute;
	efi_text_clear_screen_t ClearScreen;
	efi_text_cursor_position_t SetCursorPosition;
	efi_text_enable_cursor_t EnableCursor;
	struct efi_simple_text_output_mode *Mode;
};

struct efi_time {
	uint16_t Year;
	uint8_t Month;
	uint8_t Day;
	uint8_t Hour;
	uint8_t Minute;
	uint8_t Second;
	uint8_t Pad1;
	uint32_t NanoSecond;
	int16_t TimeZone;
	uint8_t DayLight;
	uint8_t Pad2;
};

struct efi_time_capabilities {
	uint32_t Resolution;
	uint32_t Accuracy;
	bool SetsToZero;
};

struct efi_memory_descriptor {
	uint32_t Type;
	uint64_t PhysicalStart;
	uint64_t VirtualStart;
	uint64_t NumberOfPages;
	uint64_t Attribute;
};

typedef uintptr_t __abi (*efi_get_time_t)(
	struct efi_time *Time,
	struct efi_time_capabilities *Capabilities);

typedef uintptr_t __abi (*efi_set_time_t)(struct efi_time *Time);

typedef uintptr_t __abi (*efi_get_wakeup_time_t)(bool *Enabled,
						 bool *Pending,
						 struct efi_time *Time);

typedef uintptr_t __abi (*efi_set_wakeup_time_t)(bool Enabled,
						 struct efi_time *Time);

typedef uintptr_t __abi (*efi_set_virtual_address_map_t)(
	uintptr_t MemoryMapSize,
	uintptr_t DescriptorSize,
	uint32_t DescriptorVersion,
	struct efi_memory_descriptor *VirtualMap);

typedef uintptr_t __abi (*efi_convert_pointer_t)(uintptr_t DebugDisposition,
						 void **Address);

typedef uintptr_t __abi (*efi_get_variable_t)(uint16_t *VariableName,
					      efi_guid_t *VendorGuid,
					      uint32_t *Attributes,
					      uintptr_t *DataSize,
					      void *Data);

typedef uintptr_t __abi (*efi_get_next_variable_name_t)(
	uintptr_t *VariableNameSize,
	uint16_t *VariableName,
	efi_guid_t *VendorGuid);

typedef uintptr_t __abi (*efi_set_variable_t)(uint16_t *VariableName,
					      efi_guid_t *VendorGuid,
					      uint32_t *Attributes,
					      uintptr_t *DataSize,
					      void *Data);

typedef uintptr_t __abi (*efi_get_next_high_monotonic_count_t)(
	uint32_t *HighCount);

enum efi_reset_type {
	EfiResetCold,
	EfiResetWarm,
	EfiResetShutdown,
	EfiResetPlatformSpecific
};

typedef uintptr_t __abi (*efi_reset_system_t)(
	enum efi_reset_type ResetType,
	uintptr_t ResetStatus,
	uintptr_t DataSize,
	void *ResetData);

struct efi_capsule_header {
	efi_guid_t CapsuleGuid;
	uint32_t HeaderSize;
	uint32_t Flags;
	uint32_t CapsuleImageSize;
};

typedef uintptr_t __abi (*efi_update_capsule_t)(
	struct efi_capsule_header **CapsuleHeaderArray,
	uintptr_t CapsuleCount,
	uint64_t ScatterGatherList);

typedef uintptr_t __abi (*efi_query_capsule_capabilities_t)(
	struct efi_capsule_header **CapsuleHeaderArray,
	uintptr_t CapsuleCount,
	uint64_t *MaximumCapsuleSize,
	enum efi_reset_type ResetType);

typedef uintptr_t __abi (*efi_query_variable_info_t)(
	uint32_t Attributes,
	uint64_t *MaximumVariableStorageSize,
	uint64_t *RemainingVariableStorageSize,
	uint64_t *MaximumVariableSize);

struct efi_runtime_services {
	struct efi_table_header Hdr;
	efi_get_time_t GetTime;
	efi_set_time_t SetTime;
	efi_get_wakeup_time_t GetWakeupTime;
	efi_set_wakeup_time_t SetWakeupTime;
	efi_set_virtual_address_map_t SetVirtualAddressMap;
	efi_convert_pointer_t ConvertPointer;
	efi_get_variable_t GetVariable;
	efi_get_next_variable_name_t GetNextVariableName;
	efi_set_variable_t SetVariable;
	efi_get_next_high_monotonic_count_t GetNextHighMonotonicCount;
	efi_reset_system_t ResetSystem;
	efi_update_capsule_t UpdateCapsule;
	efi_query_capsule_capabilities_t QueryCapsuleCapabilities;
	efi_query_variable_info_t QueryVariableInfo;
};

typedef uintptr_t __abi (*efi_raise_tpl_t)(uintptr_t NewTpl);

typedef uintptr_t __abi (*efi_restore_tpl_t)(uintptr_t OldTpl);

enum efi_allocate_type {
	AllocateAnyPages,
	AllocateMaxAddress,
	AllocateAddress,
	MaxAllocateType
};

enum efi_memory_type {
	EfiReservedMemoryType,
	EfiLoaderCode,
	EfiLoaderData,
	EfiBootServicesCode,
	EfiBootServicesData,
	EfiRuntimeServicesCode,
	EfiRuntimeServicesData,
	EfiConventionalMemory,
	EfiUnusableMemory,
	EfiACPIReclaimMemory,
	EfiACPIMemoryNVS,
	EfiMemoryMappedIO,
	EfiMemoryMappedIOPortSpace,
	EfiPalCode,
	EfiPersistentMemory,
	EfiMaxMemoryType
};

typedef uintptr_t __abi (*efi_allocate_pages_t)(enum efi_allocate_type Type,
						enum efi_memory_type MemoryType,
						uintptr_t Pages,
						uint64_t *Memory);

typedef uintptr_t __abi (*efi_free_pages_t)(uint64_t Memory,
					    uintptr_t Pages);

typedef uintptr_t __abi (*efi_get_memory_map_t)(
	uintptr_t *MemoryMapSize,
	struct efi_memory_descriptor *MemoryMap,
	uintptr_t *MapKey,
	uintptr_t *DescriptorSize,
	uint32_t *DescriptorVersion);

typedef uintptr_t __abi (*efi_allocate_pool_t)(
	enum efi_memory_type PoolType,
	uintptr_t Size,
	void **Buffer);

typedef uintptr_t __abi (*efi_free_pool_t)(void *Buffer);

typedef void __abi (*efi_notify_function_t)(void *Event,
					    void *context);

typedef uintptr_t __abi (*efi_create_event_t)(
	uint32_t Type,
	uintptr_t NotifyTpl,
	efi_notify_function_t NotifyFunction,
	void *NotifyContext,
	void **Event);

enum efi_timer_delay {
	TimerCancel,
	TimerPeriodic,
	TimerRelative
};

typedef uintptr_t __abi (*efi_set_timer_t)(void *Event,
					   enum efi_timer_delay Type,
					   uint64_t TriggerTime);

typedef uintptr_t __abi (*efi_wait_for_event_t)(uintptr_t NumberOfEvents,
						void **Event,
						uintptr_t *Index);

typedef uintptr_t __abi (*efi_signal_event_t)(void *Event);

typedef uintptr_t __abi (*efi_close_event_t)(void *Event);

typedef uintptr_t __abi (*efi_check_event_t)(void *Event);

enum efi_interface_type {
	EFI_NATIVE_INTERFACE
};

typedef uintptr_t __abi (*efi_install_protocol_interface_t)(
	void **Handle,
	efi_guid_t *Protocol,
	enum efi_interface_type InterfaceType,
	void *Interface);

typedef uintptr_t __abi (*efi_reinstall_protocol_interface_t)(
	void **Handle,
	efi_guid_t *Protocol,
	void *OldInterface,
	void *NewInterface);

typedef uintptr_t __abi (*efi_uninstall_protocol_interface_t)(
	void **Handle,
	efi_guid_t *Protocol,
	void *Interface);

typedef uintptr_t __abi (*efi_handle_protocol_t)(
	void **Handle,
	efi_guid_t *Protocol,
	void **Interface);

typedef uintptr_t __abi (*efi_register_protocol_notify_t)(
	efi_guid_t *Protocol,
	void *Event,
	void **Registration);

enum efi_locate_search_type {
	AllHandles,
	ByRegisterNotify,
	ByProtocol
};

typedef uintptr_t __abi (*efi_locate_handle_t)(
	enum efi_locate_search_type SearchType,
	efi_guid_t *Protocol,
	void *SearchKey,
	uintptr_t *BufferSize,
	void **Buffer);

struct efi_device_path_protocol {
	uint8_t Type;
	uint8_t SubType;
	uint8_t Length[2];
};

typedef uintptr_t __abi (*efi_locate_device_path_t)(
	efi_guid_t *Protocol,
	struct efi_device_path_protocol **DevicePath,
	void **Handle);

typedef uintptr_t __abi (*efi_install_configuration_table_t)(efi_guid_t *Guid,
							     void *Table);

typedef uintptr_t __abi (*efi_load_image_t)(
	bool BootPolicy,
	void *ParentImageHandle,
	struct efi_device_path_protocol *DevicePath,
	void *SourceBuffer,
	uintptr_t SourceSize,
	void **ImageHandle);

typedef uintptr_t __abi (*efi_start_image_t)(void *ImageHandle,
					     uintptr_t *ExitDataSize,
					     uint16_t **ExitData);

typedef uintptr_t __abi (*efi_exit_t)(void *ImageHandle,
				      uintptr_t ExitStatus,
				      uintptr_t ExitDataSize,
				      uint16_t *ExitData);

typedef uintptr_t __abi (*efi_unload_image_t)(void *ImageHandle);

typedef uintptr_t __abi (*efi_exit_boot_services_t)(void *ImageHandle,
						    uintptr_t MapKey);

typedef uintptr_t __abi (*efi_get_next_monotonic_count_t)(uint64_t *Count);

typedef uintptr_t __abi (*efi_stall_t)(uintptr_t Microseconds);

typedef uintptr_t __abi (*efi_set_watchdog_timer_t)(uintptr_t Timeout,
						    uint64_t WatchdogCode,
						    uintptr_t DataSize,
						    uint16_t *WatchdogData);

typedef uintptr_t __abi (*efi_connect_controller_t)(
	void *ControllerHandle,
	void **DriverImageHandle,
	struct efi_device_path_protocol *RemainingDevicePath,
	bool Recursive);

typedef uintptr_t __abi (*efi_disconnect_controller_t)(void *ControllerHandle,
						       void *DriverImageHandle,
						       void *ChildHandle);

typedef uintptr_t __abi (*efi_open_protocol_t)(void *Handle,
					       efi_guid_t *Protocol,
					       void **Interface,
					       void *AgentHandle,
					       void *ControllerHandle,
					       uint32_t Attributes);

typedef uintptr_t __abi (*efi_close_protocol_t)(void *Handle,
						efi_guid_t *Protocol,
						void *AgentHandle,
						void *ControllerHandle);

struct efi_open_protocol_information_entry {
	void *AgentHandle;
	void *ControllerHandle;
	uint32_t Attributes;
	uint32_t OpenCount;
};

typedef uintptr_t __abi (*efi_open_protocol_information_t)(
	void *Handle,
	efi_guid_t *Protocol,
	struct efi_open_protocol_information_entry **EntryBuffer,
	uintptr_t *EntryCount);

typedef uintptr_t __abi (*efi_protocols_per_handle_t)(
	void *Handle,
	efi_guid_t ***ProtocolBuffer,
	uintptr_t *ProtocolBufferCount);

typedef uintptr_t __abi (*efi_locate_handle_buffer_t)(
	enum efi_locate_search_type SearchType,
	efi_guid_t *Protocol,
	void *SearchKey,
	uintptr_t *NoHandles,
	void ***Buffer);

typedef uintptr_t __abi (*efi_locate_protocol_t)(efi_guid_t *Protocol,
						 void *Registration,
						 void **Interface);

typedef uintptr_t __abi (*efi_multiple_protocol_interface_t)(
	void *Handle, ...);

typedef uintptr_t __abi (*efi_calculate_crc32_t)(void *Data,
						 uintptr_t DataSize,
						 uint32_t CRC32);

typedef uintptr_t __abi (*efi_copy_mem_t)(void *Destination,
					  void *Source,
					  uintptr_t Size);

typedef uintptr_t __abi (*efi_set_mem_t)(void *Buffer,
					 uintptr_t Size,
					 uint8_t Value);

typedef uintptr_t __abi (*efi_create_event_ex_t)(
	uint32_t Type,
	uintptr_t NotifyTpl,
	efi_notify_function_t NotifyFunction,
	const void *NotifyContext,
	const efi_guid_t *EventGroup,
	void **Event);

struct efi_boot_services {
	struct efi_table_header Hdr;
	efi_raise_tpl_t RaiseTPL;
	efi_restore_tpl_t RestoreTPL;
	efi_allocate_pages_t AllocatePages;
	efi_free_pages_t FreePages;
	efi_get_memory_map_t GetMemoryMap;
	efi_allocate_pool_t AllocatePool;
	efi_free_pool_t FreePool;
	efi_create_event_t CreateEvent;
	efi_set_timer_t etTimer;
	efi_wait_for_event_t aitForEvent;
	efi_signal_event_t SignalEvent;
	efi_close_event_t CloseEvent;
	efi_check_event_t CheckEvent;
	efi_install_protocol_interface_t InstallProtocolInterface;
	efi_reinstall_protocol_interface_t ReinstallProtocolInterface;
	efi_uninstall_protocol_interface_t UninstallProtocolInterface;
	efi_handle_protocol_t HandleProtocol;
	efi_register_protocol_notify_t RegisterProtocolNotify;
	efi_locate_handle_t LocateHandle;
	efi_locate_device_path_t LocateDevicePath;
	efi_install_configuration_table_t InstallConfigurationTable;
	efi_load_image_t LoadImage;
	efi_start_image_t StartImage;
	efi_exit_t Exit;
	efi_unload_image_t UnloadImage;
	efi_exit_boot_services_t ExitBootServices;
	efi_get_next_monotonic_count_t GetNextMonotonicCount;
	efi_stall_t Stall;
	efi_set_watchdog_timer_t SetWatchdogTimer;
	efi_connect_controller_t ConnectController;
	efi_disconnect_controller_t DisconnectController;
	efi_open_protocol_t OpenProtocol;
	efi_close_protocol_t CloseProtocol;
	efi_open_protocol_information_t OpenProtocolInformation;
	efi_protocols_per_handle_t ProtocolsPerHandle;
	efi_locate_handle_buffer_t LocateHandleBuffer;
	efi_locate_protocol_t LocateProtocol;
	efi_multiple_protocol_interface_t InstallMultipleProtocolInterfaces;
	efi_multiple_protocol_interface_t UninstallMultipleProtocolInterfaces;
	efi_calculate_crc32_t CalculateCrc32;
	efi_copy_mem_t CopyMem;
	efi_set_mem_t SetMem;
	efi_create_event_ex_t CreateEventEx;
};

struct efi_configuration_table {
	efi_guid_t VendorGuid;
	void *VendorTable;
};

/**
 * @brief EFI System Table structure
 */
struct efi_system_table {
	struct efi_table_header Hdr;
	uint16_t *FirmwareVendor;
	uint32_t FirmwareRevision;
	void *ConsoleInHandle;
	struct efi_simple_text_input *ConIn;
	void *ConsoleOutHandle;
	struct efi_simple_text_output *ConOut;
	void *StandardErrorHandle;
	struct efi_simple_text_output *StdErr;
	struct efi_runtime_services *RuntimeServices;
	struct efi_boot_services *BootServices;
	uint64_t NumberOfTableEntries;
	struct efi_configuration_table *ConfigurationTable;
};

#if defined(CONFIG_X86_EFI_SYSTEM_TABLE)

/** @brief Initialize the internal storage for the EFI system table
 *
 * This will let Zephyr keeping access to the EFI system table at all time
 * after booting up.
 *
 * @param efi_sys_table The actual EFI system table pointer provided by the
 *        UEFI subsystem at boot time.
 */
void efi_init(struct efi_system_table *efi_sys_table);


#else /* CONFIG_X86_EFI_SYSTEM_TABLE */

#define efi_init(...)

#endif /* CONFIG_X86_EFI_SYSTEM_TABLE */

#endif /* _ASMLANGUAGE */

/* Boot type value (see prep_c.c) */
#define EFI_BOOT_TYPE 2

#endif /* ZEPHYR_INCLUDE_ARCH_X86_EFI_H */
