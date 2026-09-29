#include <linux/module.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>

static const struct acpi_device_id tbtn_device_ids[] = {
    {"MAT002A", 0}, // Toughpad A1/A2
    {"MAT002B", 0}, // Toughpad A1/A2
    {"MAT0035", 0}, // Toughpad FZ-M1 (single A button)
    {"", 0},
};

MODULE_DEVICE_TABLE(acpi, tbtn_device_ids);

struct tbtn_dev {
    acpi_handle handle;
    struct input_dev *input_dev;
};

/*
 * Keymap for MAT002A / MAT002B buttons (A1 + A2)
 */
static const struct key_entry tbtn_keymap[] = {
    { KE_KEY, 57, { KEY_PROG1 } }, // A1
    { KE_KEY, 67, { KEY_PROG2 } }, // A2
    { KE_END, 0 }
};

/*
 * Keymap for MAT0035 (FZ-M1, single A button)
 */
static const struct key_entry tbtn_keymap_fzm1[] = {
    { KE_KEY, 57, { KEY_PROG1 } }, // A
    { KE_END, 0 }
};

/*
 * ACPI notify handler
 */
static void tbtn_notify_handler(struct acpi_device *device, u32 event)
{
    struct tbtn_dev *tbtn = acpi_driver_data(device);
    unsigned long long hinf_result;
    acpi_status status;
    unsigned int key_value;
    unsigned int key_event_type;

    if (!tbtn || !tbtn->input_dev) {
        pr_warn("tbtn: Received notify for uninitialized device\n");
        return;
    }

    switch (event) {
    case 0x80:
        pr_info("tbtn: Notify 0x80 received\n");

        status = acpi_evaluate_integer(tbtn->handle, "HINF", NULL, &hinf_result);
        if (ACPI_FAILURE(status)) {
            pr_err("tbtn: Failed to evaluate HINF: %s\n",
                   acpi_format_exception(status));
            return;
        }

        pr_info("tbtn: HINF returned 0x%llx\n", hinf_result);

        /* Extraction : 0x39 press / 0x38 release */
        key_value      = (unsigned int)(hinf_result & 0x7F);
        key_event_type = (unsigned int)(hinf_result & 0x80);

        /* Generic mapping */
	unsigned int report_key_value = 0;
        int event_type_to_report = -1;

        /*
         * MAT002A / MAT002B :
         *  A1 : 0x39 press / 0x38 release
         *  A2 : 0x43 press / 0x42 release
         *
         * MAT0035 :
         *  A  : 0x39 press / 0x38 release
         */

        const char *hid = acpi_device_hid(device);

        if (!strcmp(hid, "MAT0035")) {
            /* FZ-M1 : single A button */
            switch (key_value) {
            case 57: /* 0x39 press */
                report_key_value = 57;
                event_type_to_report = 1;
                break;
            case 56: /* 0x38 release */
                report_key_value = 57;
                event_type_to_report = 0;
                break;
            default:
                pr_warn("tbtn: FZ-M1 unknown key_value 0x%x\n", key_value);
                return;
            }
        } else {
            /* A1/A2 */
            switch (key_value) {
            case 57: /* A1 press */
                report_key_value = 57;
                event_type_to_report = 1;
                break;
            case 56: /* A1 release */
                report_key_value = 57;
                event_type_to_report = 0;
                break;
            case 67: /* A2 press */
                report_key_value = 67;
                event_type_to_report = 1;
                break;
            case 66: /* A2 release */
                report_key_value = 67;
                event_type_to_report = 0;
                break;
            default:
                pr_warn("tbtn: unknown key_value 0x%x\n", key_value);
                return;
            }
        }

        if (!sparse_keymap_report_event(tbtn->input_dev,
                                        report_key_value,
                                        event_type_to_report,
                                        true)) {
            pr_warn("tbtn: Failed to report event: raw=0x%llx key=%u type=%d\n",
                    hinf_result, report_key_value, event_type_to_report);
        } else {
            pr_info("tbtn: Reported key event: key=%u type=%d\n",
                    report_key_value, event_type_to_report);
        }

        break;

    default:
        pr_info("tbtn: Received unknown event 0x%x\n", event);
        break;
    }
}

/*
 * add()
 */
static int tbtn_add(struct acpi_device *device)
{
    struct tbtn_dev *tbtn;
    struct input_dev *input_dev;
    int error;

    pr_info("tbtn: Device add called for %s\n", acpi_device_hid(device));

    tbtn = devm_kzalloc(&device->dev, sizeof(struct tbtn_dev), GFP_KERNEL);
    if (!tbtn)
        return -ENOMEM;

    tbtn->handle = device->handle;
    device->driver_data = tbtn;

    input_dev = devm_input_allocate_device(&device->dev);
    if (!input_dev)
        return -ENOMEM;

    input_dev->name = "TBTN Buttons";
    input_dev->phys = "tbtn/input0";
    input_dev->id.bustype = BUS_HOST;

    /* Keymap selection based on MAT0035 detection; as FZ-M1 doesn't have any MAT002A or MAT002B it might be needed to do the opposite detection */ 
    if (!strcmp(acpi_device_hid(device), "MAT0035"))
        error = sparse_keymap_setup(input_dev, tbtn_keymap_fzm1, NULL);
    else
        error = sparse_keymap_setup(input_dev, tbtn_keymap, NULL);

    if (error) {
        pr_err("tbtn: Failed to setup keymap: %d\n", error);
        return error;
    }

    tbtn->input_dev = input_dev;

    error = input_register_device(tbtn->input_dev);
    if (error) {
        pr_err("tbtn: Failed to register input device: %d\n", error);
        return error;
    }

    pr_info("tbtn: Input device registered for %s\n", acpi_device_hid(device));
    return 0;
}

/*
 * remove()
 */
static void tbtn_remove(struct acpi_device *device)
{
    pr_info("tbtn: Device remove called for %s\n", acpi_device_hid(device));
}

/*
 * ACPI driver
 */
static struct acpi_driver tbtn_acpi_driver = {
    .name = "tbtn_driver",
    .class = "tbtn",
    .ids = tbtn_device_ids,
    .ops = {
        .add = tbtn_add,
        .remove = tbtn_remove,
        .notify = tbtn_notify_handler,
    },
};

static int __init tbtn_init(void)
{
    int result = acpi_bus_register_driver(&tbtn_acpi_driver);
    if (result < 0) {
        pr_err("tbtn: Error registering ACPI driver\n");
        return result;
    }
    pr_info("tbtn: ACPI driver registered\n");
    return 0;
}

static void __exit tbtn_exit(void)
{
    acpi_bus_unregister_driver(&tbtn_acpi_driver);
    pr_info("tbtn: ACPI driver unregistered\n");
}

module_init(tbtn_init);
module_exit(tbtn_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sh1ma + matmutant");
MODULE_DESCRIPTION("TOUGHPAD ACPI TBTN A1/A2/A Button Driver");

