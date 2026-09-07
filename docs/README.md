# docs/ — architecture & design documentation

How Poseidon for AmigaOS (the 6.x line) works and where it is going: the core library and
class drivers (reverse-engineered), the context HCD ABI it now speaks, the rationale behind
that ABI and how the port from the AROS 5.x line was made.

## Reading order

| # | Document | What it is |
|---|---|---|
| 1 | [poseidon.library-architecture.md](poseidon.library-architecture.md) | The core library: object model, tasks, both edges (incl. the two-backend lower edge), locking, lifecycle, quirks. Start here. |
| 2 | [hub.class-architecture.md](hub.class-architecture.md) | The mandatory hub driver — enumeration engine, port FSM, the class-local `nh_Adr0Sema`. |
| 3 | [hubss.class-architecture.md](hubss.class-architecture.md) | Context-only SuperSpeed hub driver; documents the SuperSpeed delta and how it diverged from hub.class. |
| 4 | [hid.class-architecture.md](hid.class-architecture.md) | HID class driver internals. |
| 5 | [massstorage.class-architecture.md](massstorage.class-architecture.md) | Mass-storage class driver internals (BOT/CBI/UAS). |
| 6 | [usbaudio.class-architecture.md](usbaudio.class-architecture.md) | USB-audio class driver internals (RT-ISO). |
| 7 | [poseidon-context-hcd-abi.md](poseidon-context-hcd-abi.md) | The context HCD ABI xhci.device speaks: lifecycle ops, transfer framing, and the fast paths. |
| 8 | [poseidon-vs-xhci-driver-model.md](poseidon-vs-xhci-driver-model.md) | Design rationale: why the context ABI is shaped the way it is (Poseidon's software-managed-bus model vs xHCI's hardware-managed-device model). |
| 9 | [porting-playbook.md](porting-playbook.md) | How a component is de-AROS'd: scope (taken/dropped), the genmodule `.conf`→`.sfd` flow, `AROS_LH`→plain C, the library skeleton, the class-driver recipe, the MUI traps and the OS 3.1 floor. Read before porting an AROS fix or adding a class. |
| 10 | [rom-image.md](rom-image.md) | Image layout, the coldstart priority band. |

Historical references (original Poseidon 4.x-era documentation, kept for comparison):
[poseidon.doc](poseidon.doc), [usbclass.doc](usbclass.doc), [usbhardware.doc](usbhardware.doc),
[lowlevel.doc.patch](lowlevel.doc.patch).
