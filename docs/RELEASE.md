# 4.12.0 — Router WiFi, timed triggers and clearer radio status

## Nederlands

Meer controle over wanneer apparatuur inschakelt, hoe je de webinterface bereikt en wat er over LoRaWAN gebeurt.

- **WiFi via je router:** DHCP, instelbare opstarttijd voor een geschakelde router en AP-terugval op 192.168.4.1 als verbinden of DHCP niet lukt. Na herstel gaat het AP uit. WiFi uit blijft uit.
- **Tijdgestuurde ingangen:** per flank een minimale AAN-/UIT-tijd en een extra actievertraging. Een tegenovergestelde flank annuleert de wachtende actie; beide tijden 0 betekent direct na contactontdendering.
- **LoRa-zendsterkte:** opgeslagen plafond van 0–22 dBm, standaard 14 dBm, met apart zicht op het werkelijke zendvermogen. Houd rekening met antenne, regio en netwerkregels.
- **Radiostatus:** onderscheid tussen geaccepteerd, lokaal verzonden en ACK; laatste ontvangen RSSI/SNR met ouderdom en netwerkcontext. Nu aanmelden en Nu verzenden voor handmatige diagnose.
- **Bluetooth:** verbeterde Smart MPPT-communicatie en teruglezing, blijvend opgeslagen apparaatinstellingen en opgeschoonde statusinformatie.
- **Handleiding:** actuele NL/EN-uitleg, voorbeelden, TTN/Milesight-instellingen en een technische protocolreferentie.

**Downloads**

- **LoRaBLE-Remote-4.12.0-Windows.zip** — eerste installatie of USB-update. Uitpakken en **Install.cmd** openen. Eerste installatie regelt tijdelijk WiFi; een bestaande complete installatie wordt via USB bijgewerkt.
- **LoRaBLE-Remote-4.12.0.bin** — **Beheer → Firmware bijwerken**. Hetzelfde complete bestand zit in de Windows-ZIP.
- **SHA256SUMS** — controlesommen van beide downloads.

Instellingen blijven behouden. Controleer na de update het nieuwe zendvermogensplafond, dat op **14 dBm** begint. Handmatige radio-opdrachten slaan normale wachttijden over; neem de airtime- en netwerkregels in acht. Onderbreek de voeding niet tijdens bijwerken.

[Installatie](https://github.com/roelbroersma/victron-lorable-remote/blob/v4.12.0/docs/INSTALL.md) · [Handleiding](https://github.com/roelbroersma/victron-lorable-remote/blob/v4.12.0/docs/MANUAL.md) · [Netwerken](https://github.com/roelbroersma/victron-lorable-remote/blob/v4.12.0/docs/NETWORKS.md) · [Voorbeelden](https://github.com/roelbroersma/victron-lorable-remote/blob/v4.12.0/docs/EXAMPLES.md)

## English

More control over when equipment switches on, how you reach the web interface and what happens over LoRaWAN.

- **Router WiFi:** DHCP, configurable startup time for a switched router and AP fallback at 192.168.4.1 when connection or DHCP fails. Recovery turns AP off. Explicit WiFi off stays off.
- **Timed inputs:** independent minimum ON/OFF times and action delays per edge. The opposite level cancels a pending action; both times 0 mean immediate after input debounce.
- **LoRa transmit power:** a saved 0–22 dBm ceiling, default 14 dBm, with actual transmit power reported separately. Account for antenna, region and network rules.
- **Radio status:** accepted, locally transmitted and ACK states, plus last received RSSI/SNR with age and network context. Join now and Send now support manual diagnosis.
- **Bluetooth:** improved Smart MPPT communication/readback, persistent device settings and clearer status.
- **Documentation:** current Dutch/English guides, examples, TTN/Milesight settings and a technical protocol reference.

**Downloads**

- **LoRaBLE-Remote-4.12.0-Windows.zip** — first installation or USB update. Extract and open **Install.cmd**. First installation handles temporary WiFi; an existing complete installation updates over USB.
- **LoRaBLE-Remote-4.12.0.bin** — **Manage → Update firmware**. The Windows ZIP contains the same complete file.
- **SHA256SUMS** — checksums for both downloads.

Settings are retained. Review the new transmit-power ceiling after upgrading: it starts at **14 dBm**. Manual radio commands skip normal waiting policies; observe airtime and network rules. Maintain power throughout the update.

[Installation](https://github.com/roelbroersma/victron-lorable-remote/blob/v4.12.0/docs/INSTALL.md#english) · [Manual](https://github.com/roelbroersma/victron-lorable-remote/blob/v4.12.0/docs/MANUAL.md#english) · [Networks](https://github.com/roelbroersma/victron-lorable-remote/blob/v4.12.0/docs/NETWORKS.md#english) · [Examples](https://github.com/roelbroersma/victron-lorable-remote/blob/v4.12.0/docs/EXAMPLES.md)

© 2026 Roel Broersma. MIT project code; dependency licenses included.
