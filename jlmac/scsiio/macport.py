"""
macOS SCSI I/O for JieLi chips: speaks USB Mass Storage Bulk-Only Transport via libusb.

Only devices that look like JieLi (known VIDs or a "JieLi" manufacturer string) are ever
claimed, and the INQUIRY vendor must match the chip given in the device path, e.g. "jl:WL82".
Claiming the interface detaches the macOS mass storage driver, which needs root.
"""
import os
import struct
import usb.core
import usb.util
import usb.backend.libusb1

from scsiio.common import SCSIDevBase, SCSIException

JL_VIDS = {0x3654, 0x4C4A, 0xE2B7}
CBW_SIG = 0x43425355
CSW_SIG = 0x53425355

_backend = usb.backend.libusb1.get_backend(
    find_library=lambda name: '/opt/homebrew/lib/libusb-1.0.dylib')


def _msc_interface(dev):
    for cfg in dev:
        for intf in cfg:
            if intf.bInterfaceClass == 8 and intf.bInterfaceProtocol == 0x50:
                return intf
    return None


def _looks_jieli(dev):
    if dev.idVendor in JL_VIDS:
        return True
    try:
        mfr = usb.util.get_string(dev, dev.iManufacturer) if dev.iManufacturer else ''
    except Exception:
        mfr = ''
    return 'jieli' in (mfr or '').lower()


def _log(msg):
    """Append a line to /tmp/jlmac_debug.log (readable without root) for diagnosing connection trouble."""
    try:
        import time
        with open('/tmp/jlmac_debug.log', 'a') as f:
            f.write('%s %s\n' % (time.strftime('%H:%M:%S'), msg))
        os.chmod('/tmp/jlmac_debug.log', 0o666)
    except Exception:
        pass


def find_candidates():
    out = []
    for dev in usb.core.find(find_all=True, backend=_backend):
        try:
            if _msc_interface(dev) is not None and _looks_jieli(dev):
                out.append(dev)
        except Exception:
            pass
    return out


class SCSIDev(SCSIDevBase):
    """
    SCSI pass-through over libusb BOT, path format: "jl:<CHIPNAME>" (e.g. "jl:WL82")
    """

    timeout = int(os.environ.get('JL_TIMEOUT_MS', '5000'))

    def open(self, path):
        chip = path.split(':', 1)[1].upper() if ':' in path else None

        devs = find_candidates()
        if not devs:
            _log('no candidates')
            raise OSError('no JieLi mass storage device found')

        last_err = None
        for dev in devs:
            try:
                self._claim(dev)
            except Exception as e:
                last_err = e
                continue

            try:
                self._resync()
                vendor, product = self._inquiry()
            except Exception as e:
                self._release()
                last_err = e
                continue
            if (chip is None or vendor == chip) and product.startswith(('UBOOT', 'UDISK', 'DEVICE')):
                self.is_open = True
                self.ident = (dev.idVendor, dev.idProduct, vendor, product)
                return

            self._release()
            last_err = OSError('inquiry mismatch: %r %r' % (vendor, product))

        _log('open failed: %r (devices %s)' % (last_err, [(hex(d.idVendor), hex(d.idProduct)) for d in devs]))
        raise OSError(last_err)

    def _claim(self, dev):
        intf = _msc_interface(dev)
        self.dev = dev
        self.intf_num = intf.bInterfaceNumber
        self.detached = False

        try:
            if dev.is_kernel_driver_active(self.intf_num):
                dev.detach_kernel_driver(self.intf_num)
                self.detached = True
        except usb.core.USBError as e:
            raise OSError('cannot detach macOS driver (run with sudo?): %s' % e)

        # re-read the descriptors, the device may have been re-enumerated by the detach
        intf = _msc_interface(dev)
        usb.util.claim_interface(dev, self.intf_num)

        self.ep_in = usb.util.find_descriptor(intf, custom_match=lambda e:
            usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN and
            usb.util.endpoint_type(e.bmAttributes) == usb.util.ENDPOINT_TYPE_BULK)
        self.ep_out = usb.util.find_descriptor(intf, custom_match=lambda e:
            usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT and
            usb.util.endpoint_type(e.bmAttributes) == usb.util.ENDPOINT_TYPE_BULK)
        self.tag = 1

    def _release(self):
        try:
            usb.util.release_interface(self.dev, self.intf_num)
        except Exception:
            pass
        if self.detached:
            try:
                self.dev.attach_kernel_driver(self.intf_num)
            except Exception:
                pass
        try:
            usb.util.dispose_resources(self.dev)
        except Exception:
            pass

    def _resync(self):
        """
        Bring the Bulk-Only state machine back in step: e.g. after code we ran returns, the ROM
        sends the reply to a command the host gave up on long ago. Do a BOT reset, clear both
        endpoints and drop anything still queued on the IN pipe.
        """
        try:
            self.dev.ctrl_transfer(0x21, 0xFF, 0, self.intf_num, None, 1000)
        except usb.core.USBError:
            pass
        self._clear(self.ep_in)
        self._clear(self.ep_out)
        for _ in range(4):
            try:
                self.dev.read(self.ep_in, 512, 100)
            except usb.core.USBError:
                break

    def _inquiry(self):
        data = bytearray(36)
        saved, self.timeout = self.timeout, 2000
        try:
            self.execute(b'\x12\x00\x00\x00\x24\x00', None, data)
        finally:
            self.timeout = saved
        return str(data[8:16], 'ascii', 'replace').strip(), str(data[16:32], 'ascii', 'replace').strip()

    def close(self):
        if self.is_open:
            self._release()
            self.is_open = False

    def _clear(self, ep):
        try:
            self.dev.clear_halt(ep)
        except Exception:
            pass

    def execute(self, cdb, data_out, data_in, max_sense_len=32, return_sense_buffer=False):
        if data_out and data_in:
            raise NotImplementedError('bidirectional transfers are not supported')

        if data_out:
            dlen, flags = len(data_out), 0x00
        elif data_in:
            dlen, flags = len(data_in), 0x80
        else:
            dlen, flags = 0, 0x00

        tag = self.tag
        self.tag = (self.tag + 1) & 0xFFFFFFFF

        cbw = struct.pack('<IIIBBB', CBW_SIG, tag, dlen, flags, 0, len(cdb)) + bytes(cdb).ljust(16, b'\0')

        try:
            self.dev.write(self.ep_out, cbw, self.timeout)

            if dlen:
                try:
                    if flags & 0x80:
                        buf = self.dev.read(self.ep_in, dlen, self.timeout)
                        data_in[:len(buf)] = bytes(buf)
                    else:
                        self.dev.write(self.ep_out, data_out, self.timeout)
                except usb.core.USBError as e:
                    if e.errno != 32:   # anything but a stall is fatal
                        raise
                    self._clear(self.ep_in if flags & 0x80 else self.ep_out)

            try:
                csw = bytes(self.dev.read(self.ep_in, 13, self.timeout))
            except usb.core.USBError as e:
                if e.errno != 32:
                    raise
                self._clear(self.ep_in)
                csw = bytes(self.dev.read(self.ep_in, 13, self.timeout))

        except usb.core.USBError as e:
            raise SCSIException('USB transfer failed: %s' % e)

        if len(csw) != 13:
            raise SCSIException('short CSW (%d bytes)' % len(csw))

        sig, rtag, residue, status = struct.unpack('<IIIB', csw)
        if sig != CSW_SIG or rtag != tag:
            raise SCSIException('bad CSW: %s' % csw.hex('.'))
        if status != 0:
            raise SCSIException('command %02x failed, CSW status %d' % (cdb[0], status))

        if return_sense_buffer:
            return residue, b''
        return residue
