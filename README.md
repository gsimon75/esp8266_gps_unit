# GPS Tracker

You have some mobile things whose positions you'd like to track. Like shopping trolleys (I mean the physical ones), or gokarts,
or kids in a theme park, or pizza delivery scooters, or participants of a running competition, you name it.

The obvious choice is a mobile phone for sure, but sometimes it's just not applicable (like for the shopping trolley case), so
you need a dedicated physical device that

 - is small and light
 - runs on a rechargeable battery
 - knows its location
 - connects to some network
 - sends the location to some server

Piece of cake. Grab an ESP8266 SoC controller (it has WiFi support and a serial port), hook a GPS (like a uBlox Neo-6) to it,
read from the GPS, write to the network, a dozen lines in MicroPython and done. Or is it?

For a one-piece self-used hobby thing it is, for a _mass product_ designed to be operated by _customers_ it isn't.

If you're curious why, and why things got solved the way they are, then jump ahead to the [pesky details](#those-pesky-details),
but be warned that you'll probably get more than what you want.

If, like most of people, you'd like to see the results first, then please just read on :)


## System overview

From hardware perspective, our system contains several individual [GPS units](#gps-units) and a [Backend server](#backend-server)
only, but the software picture is a bit more complex.

1. Of course we have a [firmware](#unit-firmware) running on the units
2. The clients provide a WiFi-accessible local [config UI](#local-ui-app) for customizing the essential settings
3. The backend provides a [REST API](#backend-api) for the units, for the customer app and for the system admin app
4. So we also have a (WebView-based) [Customer app](#customer-app)
5. And a technologically similar [Admin app](#admin-app)


### GPS units

As mentioned before, it's an ESP8266 SoC, a uBlox Neo-6M GPS, an SSD1306 128x64 mono OLED display, a voltage divisor on the ADC
input, and one under-the-hood push-button for switching to local "admin mode".

In the real version a buck transformer is also needed for the 3.3 V supply.

For reusability of the platform, the hardware resource usage was kept as minimum, reserving as many pins for future extensions
as possible.


### Unit firmware

The unit is receiving the location data from the GPS, reads the voltage on its ADC input, and sends all this to the data server
via https using a REST API.

After power-on it also checks for Over-the-Air updates from an update server, and if there is a newer firmware available, then
automatically updates itself.

To make the essential settings (WiFi credentials, update server URL, data server URL, SSL credentials, etc.) configurable, the
unit can be switched to an "admin mode", in which it acts as a WiFi AP (its credentials are shown on the display as text and
as QR-code), and if the admin connects to that, it provides a web UI to manage these settings.

TODO:
For normal operation the unit name shall be shown on the display (as text and as QR-code), so the customer app can easily scan it
and thus identify the unit.

For compactness and performance reasons this firmware written in C. It uses the vendor-provided ESP8266 RTOS SDK, so it should
compile flawlessly by the standard tooling.


### Local UI app

The mentioned before, for local administration the unit acts as a WiFi AP and provides a minimal web server functionality, and
on this small web server the local config UI is accessible as a single-page web app, which then runs in a browser on the
technicians mobile phone. This way it isn't constrained by the limitations of the microcontroller of the unit, so it's written
in Javascript (VueJS + Vuetify) and it communicates with the unit via an http REST API to actually commit the changes.


### Backend API

The backend server (that the units report their locations to) is accessible also via a REST API, only this one is strictly via
https only. Actually it's three separate REST APIs:

 - One for receiving the units' reports, it uses a client-side SSL authentication scheme
 - One for providing data to the customer apps, it uses a cloud-based authentication aggregator to support client logins
   with external accounts like Google, Facebook, etc.
 - One for administration tasks, its access can be limited to the local intranet, may require client-side SSL certificate,
   and also may use any sort of authentication backend.


## Customer app

As of now, it's only a mock (around a fictive workflow for a scooter rental service) with no data connection whatsoever.

A customer app like this is inherently domain-specific, because the customers want *services* and not just raw display of
location data, so without an actual service workflow, no data presentation makes any sense.

Moreover, service workflows are usually not passive, so they rely on customer decisions and actions, like taking or returning
scooters from/to some charging stations, so it's either all mocked or none of it.


## Admin app

Part of this functionality is also domain-specific like marking some units online/offline or maintaining domain-specific status
(is a scooter charging/available/taken at the moment), but it also has generic functionalities, like

 - displaying the locations of all or selected units now or at any given time
 - displaying the paths of all or selected units within a selected time frame

TODO: implement these at least


## Those pesky details

So, the details behind this complexity :).

First of all, what WiFi shall a unit connect to (I mean the SSID and the password) ? Yours at home, or the one of the customer?

If it's going to be a _product_, then we can't expect the customer to change some lines in a µPython script and re-flash it
to the devices (and have the developer infrastructure needed for that) every time the WiFi password is changed, especially not
to _every_ device one by one, and not even keeping a developer to do that. We'll need some _local unit config process_ that is
simple and fast and straightforward enough so a technician (who has dozens of other tasks besides this one) can do it for
a hundred units in one shift, can't brick any unit by mistake (because mistakes can't be avoided) and doesn't need any
sophisticated equipment for this.

And it must be safe enough so it won't be hacked by any school kid who has a bit extra spare time for some mischief.

Next one, that "some data server" we want to send the data to, it must have some API that actually receives that data, so we'll
need a _backend_. And the Internet is a mean place, so it must have some sort of authentication, so a potential malicious
intruder can't obtain or fake that data. (Stealing the data is just one thing, disrupting the service by injecting bogus data
is also a threat.)

Suppose we have all the data on that server, valid and secure and all that, but then ... what shall we do with it? Obviously we
need some sort of _customer app_ that displays and manipulates that data (like tracking _my_ pizza order, or _my_ dog, or
take/return _my_ shopping cart, etc.) Yes, it involves more access control, and this time it's not just some company-internal
cart registry but big-time real-world external auth like sign-in with Google or Facebook or LinkedIn.

Or sign-up with plain email and password (for last resort), so we need our _own auth backend_ as well. And its administration,
like sign-up new users, confirmation emails, deleting such accounts, etc.

And then it's not just the customers who need a client, but the _administrators_, too. Like a customer is interested in
the location of _his_ pizza order or _her_ dog or where the _nearest available_ shopping cart is, but an administrator
is interested in the locations of _all_ deliveries and of _any_ dog and _all_ carts, etc. So we'll need an _admin app_ as
well.

Ugh. It's getting deeper, isn't it? And most of these things have actually nothing to do with location tracking, but they are
part of a generic _physically distributed service infrastructure_. We still need them all, but at least now you see why is such
a simple project getting so complex: because of the required non-specific infrastructure.

The good thing is that these are the same for every project, so if we do this the right way (keeping the domain-specific things
apart from the generic ones), then we'll have an infrastructure that's reusable in future projects as well.

Let's clear these things up to get the big picture piece by piece, right?


### Local unit configuration

Without WiFi the actual physical units can be accessed only physically, so at least the WiFi credentials (SSID + password) must
be locally configurable.

Some technicians will do this on a daily basis for a large number of units, so it must
 - require as few custom hardware as possible
 - be straightforward, easy, familiar, etc.
 - be secure from remote intruders
 - be secure from pokey customers

We don't want to involve custom "flasher adapters", not even USB dongles, because they would need design, testing, manufacture,
vendor support, shipping, they cost extra money, need custom software, for what OSes, they would have connectors, connector
wear-out, contact failures, connector repairs or cable replace processes, etc.

In fact, the best would be a wireless and browser-based config interface, so all the technicians need is a mobile phone, and
that's it. But that needs network, and that's what we want to configure in the first place. The chicken and the egg. 

The usual solution for this is that the units have a special _admin mode_ that is activated by some (physically covered) button
or similar thing, and in this mode the unit acts as a WiFi AP, to which the technician can connect, and access the web interface
of the unit.

The usual weak point is the SSID and the password of this admin mode AP: if it is predictable (or worse: hardwired...), then
any nearby person can access the admin mode, and if the SSID is not unique, then only one unit can be maintained at any time,
so it's not possible to have two technicians work on two units in parallel next to each other.

A random-generated SSID and password solves this, but then how to get this information to the technicians mobile?

As the units have a small *OLED display*, we can display these information, and theoretically this solves the problem.

Technically, having a human to enter two random-generated ascii garbage strings is both slow and error-prone, so it's
better to display these as *QR-code WiFi credentials*, so the technician just needs to scan it and his phone is already connected.

Then what? Can a microcontroller provide a full-fledged web interface? Yes, it can. Or rather it could, but we don't need it to.

Microcontrollers on this level are programmed in C (or in python, whatever), which is not the best language for implenting a
(more or less ergonomic) web interface. Such a code would be rigid, hard to maintain, hard to customize, error-prone, etc.

On the other hand, if the microcontroller code provides only have a small, functional, machine-only REST API in this admin mode,
then all the shiny twinkling UI can be a Single-Page Web Application (yes, html and some reactive javascript framework!) that
can be as complex as needed, because it's running on the technicians mobile in a standard browser.

From the microcontrollers perspective it's just a big static hunk of data that it shall blast through to the browser when it
asks for it, and then handle the incoming REST requests.

Of course, details within details of details, so this part is a bit more complex too :)

The ESP8266 can act as a WiFi AP, a mobile can connect to it, but then its OS (both Android and iOS) starts to complain that
"No Internet Access", which is in fact true, but that's normal, so such an error message is just confusing and it's an extra
step to have the user click it away.

Both OSes detect the presence of the "Internet Access" by trying to determine the IP address of certain FQDNs and then fetch a
web page `/generate_204` from it, and if any of these goes wrong, then comes the whining about "No Internet Access".

So, we need a DNS server to respond to these queries first... Yes, an RFC1035-compliant DNS server, on that microcontroller.
No, the SDK doesn't have any such thing, so I had to write one, see [dns-server](./unit/components/dns_server) for the implementation and
[admin-mode](./unit/component/admin_mode) for how to use it.

Then we need an http server. Fortunately that's available in the SDK and works just fine out of the box, so that was easy.
Well, sort of easy. The REST payloads are JSON documents, so using the SDK-provided `cJSON` seems obvious ... but it isn't.

It's a full DOM parser, which means that it builds the whole parse tree in memory, and memory is a scarce resource here.
And cJSON doesn't even pay the effort to put the one-of-many fields like `valuestring`, `valueint` and `valuedouble` into a
union, so it's not really for embedded platforms. And painstakingly build a recursive tree structure so immediately after
that we can painstakingly traverse it recursively again - and all this for plain *flat* JSON records of known structure...

So we needed a partial but *allocless* and statemachine-based JSON parser. Ugly as it is, but welcome to the Land of Limited
Resources :D , see the section about [memory considerations](#memory-considerations) below.

And of course there were a bunch of sweet little surprises, like scanning for available _other_ WiFi APs while we're in AP mode
isn't possible (but it doesn't trigger an error, just an empty result). There is, however, a mixed `APSTA` mode, where the
ESP8266 time-multiplexes its WiFi transceiver and simultaneously acts as an AP (towards the technicians mobile) and as a STA
(for scanning other APs). A neat corner case, which didn't make it to the SDK documentation...

Speaking of AP scanning, it's a slow process, we can't pause the REST data flow that long. So, how to solve it?
We start scanning when entering the admin mode, and when asked for the result, we present it, and immediately start a new scan.
This way the API is responsive, but still if the user reloads the list again and again, he'll get the re-scanned results.
Actually, the results of the _previous_ scan, but what's the difference :) ?
Yet another small detail that had to be thought through...


So, after all these detours, the microcontroller-side of this admin mode works like this:

1. The technician presses the *service button*, the admin mode is started
2. The microcontroller disconnects from its current WiFi AP (if connected), generates a random SSID and password, and starts
   acting like an AP
3. The SSID and the password is shown on the OLED display both textually and as a QR code
4. The technician scans the QR code with (or types the WiFi creds to) his phone

NOTE: These parts guarantee that noone without actual physical access to the unit can enter the admin mode or connect to or
redirect or sniff its WiFi traffic. Having physical access means unrestricted access to the content of the flash anyway, so
it makes no sense trying to protect it beyond this point.

5. When the technicians phone connected to this AP as a WiFi client, the microcontroller starts a DNS server and a simplistic web server

NOTE: The web server works with plain `http` only, because at this point the microcontroller doesn't necessarily knows the current
time, which is a hard requirement for TLS, so https could not be guaranteed. Moreover, https would need a server certificate and its
private key, and the technician is just about to upload these.

6. The URL of the web admin UI (served by the microcontroller) is shown both as text and as QR code
7. The technician scans the UI link QR code as well, his browser fetches the UI SPA and executes it
8. Using the menus and inputs of the UI the technician can perform the local administration tasks
   (See the details at the [Admin UI](#local-admin-ui) section below)
9. The client-side UI and the microcontroller communicate on a standard REST API
10. The admin mode ends when the technician reboots the unit via the UI

Technically we _could_ return from admin mode without reboot, but the system is partly initialized (eg. the display), partly
isn't (eg. the gps), some settings would have to be re-evaluated (those that were just changed), etc., so it's safer to reboot
the unit and do a clean startup. A persistency check with the new settings should be done anyway...


### Local Admin UI

A simple, streamlined web application that offers the following settings:

- WiFi
  - SSID (choosable from list of scanned APs or entered as free text)
  - password
- OTA ([Over-The-Air updates](#over-the-air-updates))
  - Update server URL (must be https!)
- Data server (where to report the locations)
  - URL
  - time threshold (at least one report is sent per this interval)
  - distance threshold (don't report unless moved this far since the last report)
- SSL (for the Over-The-Air updates, see later at the [PKI section](#pki))
  - Unit private key (in standard PKCS8 DER format)
  - Unit certificate (in X509 DER format)
  - OTA Server CA certificate (that signed the OTA servers cert, in X509 DER format)
- Reboot the unit

NOTE: There is no additional authentication here, because the unit must be recoverable from any inconsistent state, that
includes damage of any locally persisted data, and if there _were_ some sort of password check here, and that password _were_
damaged, then it couldn't be recovered by this admin UI.

Apart from hooking up an external flash programmer to the unit, this is the last line of repair/recovery and we mustn't assume
anything other than the firmware is running (it's serving this UI, so it's running).

Technically, this admin UI uses VueJS as framework, Vuetify as element library and Material Design as icon set, and that results
a 204k (yes, kbytes!) gzip-compressed SPA. We have at least 1M of flash on the microcontroller, so it's not even a tight fit.

It's stored in the firmware in a gzipped form (as static data) and served gzipped too, so we don't even have to decompress it.

Being an integral part of the firmware ensures a version match, so there is no such thing that a firmware is too old for a UI
or vice versa, and partial updates cannot happen either.


### Over-the-Air updates

There is no software without bugs, nor are projects whose scope never changes, so we need an easy and reliable and controllable
way to re-flash the microcontrollers firmware if needed.


#### Easy

No hardware twiddling should be needed on a per-unit basis, not even using the Local Admin UI.

1. The units themselves check for updates after a reboot (as soon as they have network) via https
2. If there is one, they flash it to a separate staging partition, not compromising the live system partition
3. A checksum is calculated for the flashed data, and if it doesn't match, the update process is aborted and a normal boot follows
4. If the update is valid, the unit is rebooted again
5. On this next boot the update is copied from the staging area to the live system partition
6. The boot process continues from the updated system
7. All locally persisted data (settings, calibration, etc.) are left intact

Details within details, again :)

The ESP8266 SDK supports having more than one `ota_N` partitions, so that copying seems to be unnecessary. It isn't, because
those flash partitions are actually mapped into the address space, so a different partition means different addresses, so
each firmware version should have to be built with addresses for the `ota_1` partition, then again with addresses for `ota_2`,
and so on. This means maintaining several builds of the same source code state (apropos, see [reproducible builds](#reproducible-builds)),
and also keeping track on what version is flashed to which partition, which one has to be overwritten during an update,
which is the next one if that write fails, etc.

Also, this would mean that partial builds would be impossible (as the LMA addresses change from one build to the other), and
this would make the development process turnaround *a lot* slower.

It was "fun" to track the startup process, that is, as much of it as possible, because the SoC registers are undocumented, the
boot ROM is undocumented, the SDK core libs are distributed in binary only. By its look the code does some sort of mapping so
there seems to be some sort of MMU, so it seems to be possible to have only one, flash-address-independent binary and map it
to the same (virtual) address, no matter where it is mapped _from_. But that's definitely out of scope now, so we have this
copy-on-next-boot method, at least that's official and therefore supported.


#### Reliable

The update server is accessed via https, its URL comes from a locally persisted setting, and the server *must have a valid
certificate*.

NOTE: A server certificate on its own proves nothing without being able to validate its Issuer, so we need the cert of its Issuer.
And the cert of the Issuers Issuer, and so on, right up to a Root CA. Normally your desktop computer does this by using a "CA
bundle", a collection of Root and Intermediate CA certs that we accept as valid. That's usually somewhere under `/etc/pki/ca-trust`,
it's a 250 kbyte long collection of 150-some certs in PEM (that is, base64-encoded) format. In DER (that is, raw binary) format
it would be about 170 kbytes, so it would easily fit in the flash...

... but not in the memory. And the SSL implementation used by the SDK (`mbedtls`) needs all CA certs used for checking to be
*loaded into RAM*. No, having them in flash, which is also addressable memory, is not possible, because they must be parsed from
their wire format (DER) into C structures, which is understandible.

What is not that understandible is that an SSL library indended for _embedded_ systems isn't capable of loading the CA certs
on-demand at the time of server cert verification. Even on server systems it's customary to
 - Scan the CA certs once at startup-time
 - Index them by some hash of their Subject field (this Subject will appear as Issuer in the server certs they have signed)
 - When a server cert needs checking, then calculate the hash of its Issuer
 - Find the matching CA cert(s) by the index
 - One by one: load them, check the Subject itself (not just the hash)
 - If it doesn't match the Issuer, then unload and proceed to the next one
 - If it matches, then do the signature check

Considering that this would require about 12 bytes per CA cert (6 byte hash, 4 byte address, 2 byte length), for 150-some certs
it would take an additional 2 kbytes of *flash*, but it's a static const data, it could remain entirely in flash, and that would
solve the problem once and for all.

Unfortunately `mbedtls` doesn't support this, it would chew up about 200 kbytes of RAM, of which we don't have even 128 kbytes
for all the system... And all this CA cert access is below several layers of abstractions, so it can't be hacked around.

That's why we need to keep in the persistent settings _the single_ CA cert that signed the OTA servers cert.

Sorry for the long and over-detailed explanation, it took even longer to figure this out and I wasn't that curious about the
internals of `mbedtls` either.

Anyhow, that's in the SDK, so that's what we use, and at least now you know the reasons, too :).

But back to validation of the OTA servers cert:

ESP8266 has no realtime clock, so its system clock starts from zero at each reboot, so at this point the unit does not
necessarily know the current time (although it may, if the GPS already got a location!), the "Not Before" and "Not After" 
validity checks may be skipped.

The updates are identified by their build timestamp, which is known both for the running and the available firmware, and if the
running one is the newer, then it would be a downgrade, so the process is aborted and a normal boot follows

The firmware revisions (build mtime, actually) are parsed from a separate descriptor file next to the firmware binary:
```
name: gps-unit.bin
mtime: 1609276792
size: 699184
fletcher16: 53659
```

You might ask why to have this descriptor file, as on the OTA server the size and the last modification timestamp is implicitely
accessible, and can be obtained by an http `HEAD` request, or moreover, if we use the [If-Modified-Since](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/If-Modified-Since)
header option, then the server either sends us the new update (if it's newer than what we have), or it sends a straightforward
`304 Not Modified` response if we're already up-to-date.

Well, on Nginx (one of the most frequently used webserver) the _default_ interpretation of that `If-Modified-Since` means
"if not modified at that exact moment". Yes, instead of ">=" they interpret it as "!=". See the docs [here](http://nginx.org/en/docs/http/ngx_http_core_module.html#if_modified_since), and they even [explain](https://trac.nginx.org/nginx/ticket/93) why they think it's a good idea...

It can be overridden, but it requires explicite nginx configuring, which most likely would be forgotten, and it would result
in malfunction as either the same update would be flashed over again and again, or no update would be flashed anytime at all.

(Level of details, I know. It was "fun" to hunt this down in the nginx source, so why would you miss it :D ?!)

But back to connecting to the OTA server:

As an SSL client, the microcontroller uses a dedicated *client-side certificate*, which is checked by the server (see the
[nginx site definition](./backend/pki/backend.wodeewa.com)).

Although "security by obscurity" is no security at all, this adds at least a layer of obscurity, so firmware binaries are
published only for the units.

The second reason is that client-side certificates provide a hard-to-spoof way to identify individual units, what we will
definitely need for handling the location reports, but it also comes handy at the OTA phase too:


#### Controllable

As the units' client-side certificates contain unique and reliable unit identification (i.e. the "Subject" field of the cert),
the server can distinguish based on this, and the units' requests can be redirected or rewritten even on a per-unit basis, a handy
thing for a gradual rolling update or an A-B testing, all of which requiring only one server-side configuration change.


### GPS operation

Remember that "... just hook a GPS to it ..."? Is it a big surprise, that it's not that simple as it sounds :) ?

The first thing is that without some _network-obtained_ data about satellite trajectories (see [AGPS](https://en.wikipedia.org/wiki/Assisted_GPS))
it takes quite some time while the GPS receiver obtains it from the satellite downlink, sometimes even minutes.

What shall we do while we don't have a valid GPS fix? Shall we just report the battery status every N seconds? Now we do.

As of obtaining the almanac from the network, it's problematic. We don't have a realtime clock, so until the first GPS fix
we don't know the current time either. But without a valid time we cannot trustworthily verify an https certificate either,
because we can't tell if it's expired or not. So we can't verify the authenticity of the server from where we would bring
the almanac data, so it can be tampered with, we can't trust it.

After receiving the first GPS fix, we know the time, but then we don't need the almanac any longer.

Then we can set the system clock to the timestamp got from the GPS, which will be _somewhat_ accurate.
Only somewhat, because it takes time for the GPS to send that data to us through a serial link: some hundreds of bits, at
9600 bps by default, or 230400 if we switch to that.

To reduce the transmission delay we switch the Neo-6 to a vendor-specific UBX binary protocol and enable only those messages
that are actually needed for location and timestamp data (i.e. no satellite status info, etc.) The code still contains the
standard NMEA parser, so if a different GPS chip is used, all it takes is to toggle a `#define` in [gps.c](./gps.c).

NOTE: The uBlox Neo-6 GPS could emit a _timepulse_, a separate signal that marks the exact time of the reception, with that
we could get sub-microsecond accuracy ... but those wires aren't published on the usual GPS drop-in modules.
On the other hand, on a purpose-designed _product_ we can very well connect that timepulse output to an input of the
microcontroller, and then we'll really achieve that clock accuracy. It'll require some coding, but nothing problematic.

So now we set the system clock to the _reception time_ of the data packet, that's still below 5 ms delay, so we can live with
that. Also, the microcontroller is running at _about_ 160 MHz, but not exactly, so without feedback the system clock can be
slightly slower or faster than the real time, and this drift accumulates.

Abruptly resetting it to the GPS from time to time is a bad idea, because such "jumps" in time tend to spoil timeouts and
delays, especially when resetting the system time _backwards_, so timed events may occur twice, negative values may result
where the code isn't prepared for that, and so on.

The solution is that if we detect that the system clock is faster than realtime, then we notify the system scheduler that it
should assume more ticks per µs, and if it's too slow, then it should assume less. However, this setting is discrete, (like 159
or 160 or 161 MHz, but not the fractionals), so we can't adjust it to the real clock speed, it'll be always either too fast
or too slow.

So we're doing this clock speed adjustment continuously, switching between 159, 160 and 161 back and forth, keeping the drift
as close to zero as possible. Now it's amplitude is about 1 ms, and that includes all uncertainties in the serial receiving
(it's delayed during WiFi events, for example), so it's quite acceptable without that timepulse signal.

Keeping the system clock synced with the GPS lets us do time-related tasks (like certificate validity checking) even when
temporarily losing the GPS signal, and it may be essential for other data-collecting applications.

I've said that it takes time for the data packets to get through from the GPS, and the higher the serial speed the lower this
delay is, so we switch the GPS from 9600 bps to 230400 bps... and open a can of new problems with that :).

After power-on the GPS is set to 9600 bps (we could change that, but then every unit would need some pre-installment setup
procedure, and we don't want that), but in the first steps we (non-permanently) switch it to 230400 bps, and it's reset only
by a power cycle, and by a microcontroller reset or restart *it isn't*. So when our code starts, it may find the GPS still
at 9600, or already at 230400. We must test for both: if there is no data coming for 3 seconds (normally a packet comes every
second), then we switch to the next speed option and try listening on that one.

Remember about switching the GPS to the UBX protocol? Same issue here again: at power-up the GPS sends us 6 types of NMEA
messages, but we change it to send 3 types of UBX messages instead. After a reset or a restart we start anew, but the GPS
stays as it is, so we'll start by receiving UBX messages and no NMEA. Therefore we check for all of them, and parse whatever
messages we receive.


### Memory considerations

To begin with, Espressif tells in no documents even the exact amount of RAM on the ESP8266, it's part of their
"hardware + ROM + SDK core libs" concept that their public interface is the C headers of those core libs, and one shouldn't
rely on anything beyond that.

I quote the most informative paragraph from the product datasheet:

    ... when ESP8266EX is working under the Station mode and connects to the router, the maximum programmable space accessible
    in Heap + Data section is around 50 kB. ...

([Unofficially](https://github.com/esp8266/esp8266-wiki/wiki/Memory-Map#memory-layout) it seems to have 96k data-ram and
32k instruction-ram)

In addition to (or rather substraction from) that is that for each SSL connection `mbedtls` eats up about 15k as well.
And the RTOS also has quite a lot of memory overhead, stacks for each system task and internal buffers for TCP/IP and so on.

Running out of memory can result various nice errors, from straightforward stack overflows to subtle "-0x4310"-like mbedtls
errors (this particular one means memory allocation error during private key loading, btw.). The firmware code is supposed to
check and handle every dynamic allocations, but not all the SDK component libraries do, so wasting memory is generally a
"very bad idea (tm)" here...

After some runtime diagnostics a lot of SDK parameters have been finetuned, including the stack sizes of the event loop (0.5k
gain here), the idle task (another 0.5k), the timer task (1k gain), the main task (1.5k), the tcp/ip task (0.5k), the WiFi ppt
task (0.5k). This step needed some trickery, as some of these settings have overly high minimal limits, so `sdkconfig` doesn't
accept any value. Fortunately the `sdkconfig` items are collected from various places that are sorted alphabetically and the
first definition wins, so all we needed was an otherwise empty [component](./unit/components/_Kconfig_overrides/Kconfig)
(it also contains examples how to add custom menus and items).

Also note that in FreeRTOS when a task finishes, its stack is freed up by the idle task, so until it gets to run, there is no
garbage collection, so we had to add explicite sync points in the process to wait for the idle task and its `prvCheckTasksWaitingTermination()`.

With this we have about 16k free during normal operation, but this required another unusual practice: holding resources only
as long as we actually need them: seemingly unnecessary blocks within functions

```
    ...
    {
        uint8_t something[32];
        ...
    }
```

If we don't need that array in the whole function, then we shouldn't keep it allocated from the stack all the time. Even such
things make a difference here!

Also, the "copy a string to another string" practice shall be avoided wherever possible. Allocate it once, and if splitting
at some delimiters, the zap those delimiters with zeroes, and presto, there are the items sliced apart nicely, in the same
buffer. The funny thing is that this practice can result more reliable codes, as it has less external dependencies, but it also
requires more efforts to write and to read it as well.

At quite a number of places the code contains "ugly" things like this, but usually with a good reason. `malloc` is not our
friend here, and even if it seems that some bytes could be wasted here and there for code-aesthetics, they might mean an OOM
situation when we add the next functionality. And it's far better to write a minimalistic code in the first place, then to
hunt for freeable bytes in older codes when we are OOM later.

So please read the code with keeping in mind the fact that here the whole heap is below 50k, task stacks are measured in
100s of bytes, and it's not at all like the "don't care about leaks, the user will restart it" desktop world.


### Reproducible builds

As we're distributing firmwares in a binary form, we must be able to reproduce any version anytime.
It's a [well documented](https://reproducible-builds.org/docs/) subject, take some time to skim through those docs first.

Unfortunately the [official SDK](https://github.com/espressif/ESP8266_RTOS_SDK.git) that we're using is ripe with sources
of irreproducibily: a bunch of source ordering issues and some timestamps as well.

I've tracked down such issues until now the builds are reproducible. For details see [the patch](./unit/misc/ESP8266_RTOS_SDK.reproducible.diff) 

The timestamps that get hardwired into the binaries brought in a design question: what should those "source epoch"
timestamps represent? To be reproducible, fully committed (no local change) sources should use the last commit timestamp,
but sources with local changes should use something newer, like the current time.

Which opens another can of worms: in case of a partial build, what would happen with the timestamps hardwired in modules
that are _not_ rebuilt? Well, they cannot remain unchanged, so they must be rebuilt forcibly. That's why those source
epoch timestamps in this project aren't just some defined symbols, but they got a separate global variable defined by a
separate module [epoch.c](./unit/components/misc/epoch.c).

Details within details again: if you look at the [Makefile](./unit/Makefile) you'll wonder why that complicated rain dance about removing
`epoch.o` and not just make it `PHONY`. The SDK uses a multi-level make system where the Makefiles aren't including one another,
but there are nested `make` calls. The objects like `epoch.o` are built on some deeper level, so defining it PHONY on the top
level wouldn't do anything at all. (Yes, it was "fun" to track this, too.)


### ESP8266 hardware details

First of all, to boot from flash, GPIO0 and GPIO2 are pulled up and GPIO15 is pulled down by 10k external resistors. Right
after the boot starts, these pins can be (and are...) reused, but the pull-up/down resistors stay connected, so this limitation
must be considered.

For selecting flashing mode, a pushbutton is used to pull down GPIO0 *before power-up*. This means that it can't be reused
to connect anything that would prevent it from being pulled to GND, or that couldn't take being pulled to GND, or that would
pull it down inadvertantly before a reset.

Typically it's reused for some non-essential active-low output (like a status LED), but as it already has that nice pull-down
button connected, we reuse it as a plain digital input, and pulling it low by the button *when we're already running* activates
the admin mode, so we're not wasting another pin just for that.

The serial lines that connect the GPS have a sort of unorthodox setup.

The only bi-directional UART of ESP8266, UART0, by default works on TX=GPIO1 and RX=GPIO3, but during bootup the ROM code is
dumping logs on it (on 74880 bps) that cannot be turned off and it shouldn't get to the GPS. Not mentioning the last-resort
serial flashing traffic... Neither should the GPS output interfere with that flashing data, so wiring the GPS to these GPIO1
and GPIO3 permanently is not a good idea.

Fortunately, the ESP8266 has a feature to swap its U0TXD with U0RTS (a.k.a. GPIO15) and U0RXD with U0CTS (GPIO13), though it
must be activated by our firmware. (Note that GPIO15 has already limitations due to being a boot selector pin: it's pulled
down already, so it's not a big loss to use it for this reassigned serial line.)

So the GPS is connected to these _alternate_ UART0 pins TX=GPIO15, RX=GPIO13, and until our firmware activates this swapping,
it's completely isolated, all boot logs and emergency flashing traffic goes on the default GPIO1 and GPIO3. After the swapping,
all the UART functions work as usual on the new pinout, but at that time the boot phase is long passed, so noone will interfere
with our GPS traffic.

Except for maybe our own logging, as the firmware also generates (a lot of) logging output, and the only meaningful way is
to send it to _a_ UART. Logs are a write-only thing, so they don't require a bi-directional UART, so the second, transmit-only
UART of the ESP8266 will do nicely. (In fact, that's a full UART too, but its RX pin is permanently used for accessing the
flash, so we can't use it.)

So the SDK is configured to send all system logs to UART1, and then it goes to GPIO2. We've already mentioned this GPIO2:
it is one of the boot selector pins and is pulled up by a resistor and before boot must not be pulled down, so it has a
limited usability. Moreover, _during_ bootup that 74880-baud startup log is sent not just to U0TXD, but to this U1TXD as
well, so its usefulness is further limited by having this junk sent to it at startup.

Therefore it's not a big waste to dedicate it for a log output, although if needed, these logs can be completely disabled
in the SDK config.

The SSD1306 display is connected via I²C, SDA=GPIO4, SCL=GPIO5, nothing fancy about this one :)

The ADC input can handle voltages only up 1V, so to extend its range to 3.3V it requires a 2-resistor voltage divisor:
22 kOhm up to the real input and 10 kOhm down to GND. That's a 10/(10+22) divisor, which is almost the 1/3.3 what we need.

If the analog input range is different, then this divisor should be recalculated accordingly, like for a 4.2V range (Li-Ion)
it should be 1.0/4.2, so its 33k towards the real input and 10k to GND.

Conclusion:
 - GPIO0 has a pull-down pushbutton that *before* power-on activates the flashing mode (by the ROM), and *runtime* it
   activates the admin mode (by our firmware)
 - GPIO1 and GPIO3 are bootup-time UART0, used for bootup output and for flashing, in runtime they are free for reuse to connect
   to the input of any device that can take some random junk data (eg. status LEDs)
 - GPIO2 is UART1 TX for our runtime logging
 - GPIO4 and GPIO5 are I²C lines, used for the OLED display
 - GPIO15 and GPIO13 are normal runtime UART0, used for the GPS
 - ADC is connected to a voltage divisor that is connected to the analogue input (like the battery)
 

What's remaining:
 - GPIO1 for non-essential outputs (like status LEDs)
 - GPIO3 for non-essential outputs (like status LEDs), but if we cease supporting the emergency flashing, then it's completely available
 - GPIO12, GPIO14 completely available, have internal pull-up and pull-down resistors too
 - GPIO16 completely available, has only internal pull-down (can be used to wake up from deep sleep, not that we used that)


### PKI

If you're not familiar with identity validation, specifically with X.509 certificates and its concepts and workflows, then it's
high time to read up on the matter. This README is already long enough, so I won't include it here ("the margin is too narrow
for that" :D ), the keywords are: encryption, asymmetric encryption, hashes, digital signatures, X.509 certificates, CA chain,
root CAs, CRLs.

This section is only about how we _use_ this technology, not about how it _works_.

So, we are talking about the following SSL-wrapped communication channels:


#### Unit and "OTA" server

In the demo the FQDN of this "OTA" server is `backend.wodeewa.com`, the path within it is `/ota/`.

The OTA server must have a valid certificate and the cert of the trusted CA that has signed it must be uploaded to the
units via the local admin UI.

Normally a Root CA bundle would be enough, but `mbedtls` can't do on-demand CA cert loading and we don't have the RAM to
load all the CA certs, so that's it.

On the other hand, this makes it possible to use company-internal certs for the OTA server (it's not intended for public
after all), in which case this company-CA cert needs to be installed on the units.

The OTA server cert won't always be checked for time validity, it depends on whether the unit has a current time (from the GPS)
or not yet.

On the client-side the unit *optionally* uses a client certificate, so this client cert and its private key must also be uploaded to it.
If there is no client certificate, then the unit will connect to the OTA server with a freshly generated ephemeral key and send
no certificate, but then the OTA server cannot restrict access to the firmware update files for the units only, and it also
cannot distinguish between units, so rolling updates and A-B testing won't be possible.

Therefore using a unique client-side certificate on each unit is highly advisable.

These client-side certs may be signed by any CA, it can be a company-, department- or even project-level "fake" CA, it
matters only at the OTA server: it must know the certificate of this CA so it can validate the certs of the connecting units.

The unit is identified by the "CN=..." attribute of the Subject field of its certificate, that's how it's possible to serve a
different update for _some_ chosen set of units than for the rest.

Example for generating such a local-level "fake" CA:
```
openssl req -days 3650 -x509 -extensions v3_ca -newkey rsa:4096 -keyout fake_ca.key -out fake_ca.crt
openssl rand -hex 16 >fake_ca.srl
```

Example for generating a private key and a certificate for a unit:
```
openssl genrsa -out gps_unit_0.key 4096
openssl req -new -key gps_unit_0.key -out gps_unit_0.csr

openssl x509 -req -CAkey fake_ca.key -CA fake_ca.crt -CAserial fake_ca.srl -days 1200 -in gps_unit_0.csr -outform der -out gps_unit_0.crt.der

openssl pkcs8 -in gps_unit_0.key -outform der -nocrypt -out gps_unit_0.key.der
```
(The files `gps_unit_0.csr` and `gps_unit_0.key` are no longer needed, they may be deleted.)

To test/access the OTA server from desktop:
```
curl -v --key-type DER --key gps_unit_0.key.der --cert-type DER --cert gps_unit_0.crt.der https://ota.wodeewa.com/ota/gps-unit.desc
```

#### Unit and "Backend" server

In the demo the FQDN of this "Backend" server is `backend.wodeewa.com`. (Technically it's the same as the "OTA" server, but
that's not a requirement.)

Pretty much the same as with the OTA server. It even expects the Data server cert to be signed by the same CA as for the OTA
server cert, which in practice doesn't necessarily has to be the same, so now it's a known limitation.

If it's really needed, we can introduce a new setting for _another_ CA cert, but the real solution would be the on-demand
loading of CA certs, and although I'm working on that, it has a very low prio...

The unit is identified by the "CN=..." attribute of the Subject field of its certificate here too, so that's how its reports
will appear in the database.


#### Customer app and "Client" server

In the demo the FQDN of this "Client" server is `client.wodeewa.com`.

That customer app is actually a web app, wrapped in a hybrid webview layer by CapacitorJS, so it's like a web app that runs in a
browser on the customers mobile.

If the customer don't want to install this Android/iOS app, it the can be opened right from this "Client" server, it's more
traffic and slower starting, otherwise it's the same. (Actually it can be opened even from a desktop browser, but for a
location-tracking client it makes very little sense...)

Because it must be accessible online from anywhere, this server must have a valid server certificate, otherwise the clients
get security warnings, and its domain name is also required for the [authentication backend](#client-authentication) configuration.

As customers may connect from anywhere, no client-side certificate check is performed by this server, _the app itself_ is served
for anyone.

On the other hand, _access to the service_ is restricted to signed-up customers only and therefore it requires a
[client authentication](#client-authentication).

The app communicates with the service REST API on this same server also via https, and for load balancing support it also
uses session cookies.

Theoretically it would be possible to generate client-side certificates for the customers upon sign-up and subscription
renewal, but it would be still too much of technical hassle for the customers:
 - they should generate a private-public key pair
 - they should fill in the details for a Certificate Signing Request (.csr)
 - send in the .csr some way to our "fake" CA
 - our "fake" CA should validate **SOMEHOW** that the sender is indeed who they tells they are (authentication)
 - our "fake" CA should check that the sender is indeed entitled to that certificate, eg. has paid for the service (authorization)
 - our "fake" CA should sign that .csr into a .crt certificate
 - send it back to the customer
 - they should install that cert (and the corresponding privte key) to their browsers keystore (or to the app if they uses that one)

First of all, the customers pay for a service and not for a comms security exercise, so they would rightfully refuse to do this
manually, and that would cross out the online browser support.

Second, on most mobile or laptop platforms (ChromeOS, CloudReady, Android, most probably iOS as well) the browsers don't have
their own key/cert store, but they use the vault provided by the OS, it's several levels deep in the system settings (like
Settings / Security / Credential storage / User credentials / Install from SD card), and for security reasons it can't be
automated, so it's indeed a lengthy struggle.

And finally, while actually signing that request (above at that "SOMEHOW") we have no way to verify the identity of the
applicant, that it's indeed the rightful owner of the subscription who's asking us to sign his cert.

Actually, we do have ways to _have it verified_ by some external auth provider (like Google, Facebook, etc., the provider of
that account), but then we don't need the hassle with the certificate, we can (and do) just use this auth backend.



#### Admin app and "Admin" server

In the demo the FQDN of this "Admin" server is `admin.wodeewa.com`.

It's basically just a second web app with different actions, so all the "Client" server things above can be applied here as
well ... maybe with some differences. 

First of all, its access can be restricted to the site intranet, because that's where the maintenance work is limited to.

Then, we can't expect the paying customers to generate keys and install certificates to credential vaults and so on, but we
quite well can expect paid administrators and technicians to do that, if its benefits are worth us more than the efforts it
requires:

 - Having a work contract with us, their identity is checked and valid enough
 - Being present at the workplace, they can hand in the .csr and we can hand them their certificate in an absolutely secure way
 - Their administrative access is separated from the external accounts, so even if their social account is locked, or deleted,
   or compromised, it doesn't affect neigher the security of our system nor their ability to work
 - Their social account may be logged in on several devices (like their desktops at home), but if these certificates are
   installed only on one device, then it cannot be used by another person
 - Credential vaults are always identity-protected (password or lock pattern or fingerprint or other biometric sensors), so
   they are more secure than a social media account

So, if there is a need for it, this "Admin" server can also be protected by client-side certificate check.

If even more security and authenticity is required, the staff can be equipped with smartcard readers (like [ACR3901U-S1](https://www.amazon.com/ACR3901U-S1-Bluetooth%C2%AE-Contact-Card-Reader/dp/B01BX4GKAK)),
and can use industry-standard smartcards to (irretrievably) generate the private key, provide the .csr and store the
certificate.

Although... this sort of defeats the principle of not requiring specific hardware... but still it's an option.


### Client authentication

[Firebase](https://firebase.google.com/) is an authentication backend integrator service, it provides a secure way to use
Google, Facebook, Apple, etc. account providers as to authenticate client logins in a secure and verifiable way.

I don't want to copy their whole document store here, so only the key features:
 
 - The client authentication is done by the vendor-provided popups and forms, the client credentials are never entered to
   the app that requests the auth process
 - If the client is protecting his account by methods more sophisticated than a plain password (2FA, like authenticator
   agents or OTP passcode tokens, etc.), it's no problem, because it's the account provider who's performing the auth process
 - The app has to be registered at the account provider, so the client is clearly shown what service he is asked to
   authenticate for
 - Upon successful authentication, the requestor app gets only a signed token from the account provider that contains the
   ID of the user, the ID of the service and the validity timeframe, so a token cannot be reused indefinitely and a token
   granted for one service cannot be used to access a different one
 - This token doesn't contain any user secrets, not even in an encrypted form, so there is no way to retrieve user secrets
   from such a token
 - The app can pass this token to the REST backend server, and that server can check the token with the account provider,
   and iff it confirms the validity of the token, then the server can open a session for that user and return a session cookie
   or use whatever session-binding method it prefers
 - Worst case the app can send the token in every of its requests and the server can verify it every time, although it's an
   expensive practice, so usually it's enough to perform it only before the most sensitive operations like sign-in or
   account deletion.


## nginx config

So we have three "servers" (in the good ol' apache era we would've called them "vhosts")
 - [Backend](./unit/backend/pki/backend.wodeewa.com)
 - [Client](./unit/backend/pki/client.wodeewa.com)
 - [Admin](./unit/backend/pki/admin.wodeewa.com)

 The "Admin" server is currently very similar to the "Client" server, only the paths are different.

The common things: SSL only, same server cert and private key, same SSL settings, same gzip settings.

Both servers have a `/v0` path-prefix that is routed to the common REST backend, although with different path structure: `client.wodeewa.com/v0` becomes `/client/v0` and `backend.wodeewa.com/v0` becomes `/backend/v0`.

Note that `nginx` is performing the SSL termination, and in order to keep the original connection info, it passes them on as `X-Whatever` header fields.

The other paths on both servers point to plain static content.

The differences:

The "Backend" server has a client-side certificate checking configured (`ssl_verify_client`, `if ($ssl_client_verify != SUCCESS) ...`)

**Temporarily** the "Client" server has a basic user+pass auth configured to prevent internet-crawlers and uninvited visitors to accidentally access the under-development components.

The static content of the "Backend" server are the OTA firmwares (descriptor file + binaries), the static content of the "Client" server is the Customer app.
