/***************************************************************************
* Copyright (c) 2013 Nikita Mikhaylov <nslqqq@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the
* Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
***************************************************************************/

#include <QtCore/QDebug>
#include <QtCore/QObject>

#include "KeyboardModel.h"
#include "KeyboardModel_p.h"
#include "KeyboardLayout.h"
#include "XcbKeyboardBackend.h"

#include <QSocketNotifier>

#include <X11/XKBlib.h>

namespace SDDM {
    XcbKeyboardBackend::XcbKeyboardBackend(KeyboardModelPrivate *kmp) : KeyboardBackend(kmp) {
    }

    XcbKeyboardBackend::~XcbKeyboardBackend() {
    }

    void XcbKeyboardBackend::init() {
        connectToDisplay();
        if (d->enabled)
            initLedMap();
        if (d->enabled)
            initLayouts();
        if (d->enabled)
            initGroups();
        if (d->enabled)
            initState();
    }

    void XcbKeyboardBackend::disconnect() {
        delete m_socket;
        xcb_disconnect(m_conn);
    }

    void XcbKeyboardBackend::sendChanges() {
        xcb_void_cookie_t cookie;
        xcb_generic_error_t *error = nullptr;

        // Compute masks
        uint8_t mask_full = d->numlock.mask | d->capslock.mask,
                mask_cur  = (d->numlock.enabled  ? d->numlock.mask  : 0) |
                            (d->capslock.enabled ? d->capslock.mask : 0);

        // Change state
        cookie = xcb_xkb_latch_lock_state(m_conn,
                    XCB_XKB_ID_USE_CORE_KBD,
                    mask_full,
                    mask_cur,
                    1,
                    d->group_id,
                    0, 0, 0);
        error = xcb_request_check(m_conn, cookie);

        if (error) {
            qWarning() << "Can't update state: " << error->error_code;
        }
    }

    void XcbKeyboardBackend::connectToDisplay() {
        // Connect and initialize xkb extension
        xcb_xkb_use_extension_cookie_t cookie;
        xcb_generic_error_t *error = nullptr;

        m_conn = xcb_connect(nullptr, nullptr);
        if (m_conn == nullptr || xcb_connection_has_error(m_conn) > 0) {
            qCritical() << "xcb_connect failed, keyboard extension disabled: " << xcb_connection_has_error(m_conn);
            d->enabled = false;
            return;
        }

        // Initialize xkb extension
        cookie = xcb_xkb_use_extension(m_conn, XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);
        xcb_xkb_use_extension_reply(m_conn, cookie, &error);

        if (error != nullptr) {
            qCritical() << "xcb_xkb_use_extension failed, extension disabled, error code"
                        << error->error_code;
            d->enabled = false;
            return;
        }
    }

    void XcbKeyboardBackend::initLedMap() {
        // Get indicator names atoms
        xcb_xkb_get_names_cookie_t cookie;
        xcb_xkb_get_names_reply_t *reply = nullptr;
        xcb_generic_error_t *error = nullptr;

        cookie = xcb_xkb_get_names(m_conn,
                XCB_XKB_ID_USE_CORE_KBD,
                XCB_XKB_NAME_DETAIL_INDICATOR_NAMES);
        reply = xcb_xkb_get_names_reply(m_conn, cookie, &error);

        if (error) {
            qCritical() << "Can't init led map: " << error->error_code;
            d->enabled = false;
            return;
        }

        // Unpack
        xcb_xkb_get_names_value_list_t list;
        const void *buffer = xcb_xkb_get_names_value_list(reply);
        xcb_xkb_get_names_value_list_unpack(buffer, reply->nTypes, reply->indicators,
                reply->virtualMods, reply->groupNames, reply->nKeys, reply->nKeyAliases,
                reply->nRadioGroups, reply->which, &list);

        // Get indicators count
        int ind_cnt = xcb_xkb_get_names_value_list_indicator_names_length(reply, &list);

        // Loop through indicators and get their properties
        QList<xcb_get_atom_name_cookie_t> cookies;
        for (int i = 0; i < ind_cnt; i++) {
            cookies << xcb_get_atom_name(m_conn, list.indicatorNames[i]);
        }

        for (int i = 0; i < ind_cnt; i++) {
            QString name = atomName(cookies[i]);

            if (name == QLatin1String("Num Lock")) {
                d->numlock.mask = getIndicatorMask(i);
            } else if (name == QLatin1String("Caps Lock")) {
                d->capslock.mask = getIndicatorMask(i);
            }
        }

        // Free memory
        free(reply);
    }

    void XcbKeyboardBackend::initLayouts() {
        d->layouts << new KeyboardLayout(QLatin1String("aa"), QLatin1String("Set by X"));
        d->layouts << new KeyboardLayout(QLatin1String("gb"), QLatin1String("Gemini English (UK)"));
        d->layouts << new KeyboardLayout(QLatin1String("us"), QLatin1String("Gemini English (US)"));
        d->layouts << new KeyboardLayout(QLatin1String("se"), QLatin1String("Gemini Swedish"));
        d->layouts << new KeyboardLayout(QLatin1String("fi"), QLatin1String("Gemini Finnish"));
        d->layouts << new KeyboardLayout(QLatin1String("de"), QLatin1String("Gemini German"));
        d->layouts << new KeyboardLayout(QLatin1String("at"), QLatin1String("Gemini Austrian"));
        d->layouts << new KeyboardLayout(QLatin1String("fr"), QLatin1String("Gemini French"));
        d->layouts << new KeyboardLayout(QLatin1String("be"), QLatin1String("Gemini Belgian"));
        d->layouts << new KeyboardLayout(QLatin1String("pt"), QLatin1String("Gemini Portuguese"));
        d->layouts << new KeyboardLayout(QLatin1String("ru"), QLatin1String("Gemini Russia"));
        d->layouts << new KeyboardLayout(QLatin1String("jp"), QLatin1String("Gemini Japan"));
        d->layouts << new KeyboardLayout(QLatin1String("ara"), QLatin1String("Gemini Arabic"));
        d->layouts << new KeyboardLayout(QLatin1String("cz"), QLatin1String("Gemini Czech"));
        d->layouts << new KeyboardLayout(QLatin1String("hr"), QLatin1String("Gemini Croatian"));
        d->layouts << new KeyboardLayout(QLatin1String("no"), QLatin1String("Gemini Norwegian"));
        d->layouts << new KeyboardLayout(QLatin1String("dk"), QLatin1String("Gemini Danish"));
        d->layouts << new KeyboardLayout(QLatin1String("dvorak"), QLatin1String("Gemini English (Dvorak)"));
        d->layouts << new KeyboardLayout(QLatin1String("gr"), QLatin1String("Gemini Greek"));
    }

    void XcbKeyboardBackend::initGroups() {
        xcb_xkb_get_names_cookie_t cookie;
        xcb_xkb_get_names_reply_t *reply = nullptr;
        xcb_generic_error_t *error = nullptr;

        // Get atoms for short and long names
        cookie = xcb_xkb_get_names(m_conn,
                XCB_XKB_ID_USE_CORE_KBD,
                XCB_XKB_NAME_DETAIL_GROUP_NAMES | XCB_XKB_NAME_DETAIL_SYMBOLS);
        reply = xcb_xkb_get_names_reply(m_conn, cookie, &error);

        if (error) {
            // Log and disable
            qCritical() << "Can't init groups: " << error->error_code;
            return;
        }

        // Unpack
        const void *buffer = xcb_xkb_get_names_value_list(reply);
        xcb_xkb_get_names_value_list_t res_list;
        xcb_xkb_get_names_value_list_unpack(buffer, reply->nTypes, reply->indicators,
                reply->virtualMods, reply->groupNames, reply->nKeys, reply->nKeyAliases,
                reply->nRadioGroups, reply->which, &res_list);

        // Get short names & store initial x11 value
        QString x11Symbols = atomName(res_list.symbolsName);
        QList<QString> short_names = parseShortNames(x11Symbols);
        if (d->x11Symbols.length() == 0) {
            d->x11Symbols = x11Symbols;
        }

        // Loop through group names
        d->groups.clear();
        int groups_cnt = xcb_xkb_get_names_value_list_groups_length(reply, &res_list);

        QList<xcb_get_atom_name_cookie_t> cookies;
        for (int i = 0; i < groups_cnt; i++) {
            cookies << xcb_get_atom_name(m_conn, res_list.groups[i]);
        }

        for (int i = 0; i < groups_cnt; i++) {
            QString nshort, nlong = atomName(cookies[i]);
            if (i < short_names.length())
                nshort = short_names[i];

            qCritical() << nshort << nlong;
            d->groups << new KeyboardLayout(nshort, nlong);
        }

        // Free
        free(reply);
    }

    void XcbKeyboardBackend::initState() {
        xcb_xkb_get_state_cookie_t cookie;
        xcb_xkb_get_state_reply_t *reply = nullptr;
        xcb_generic_error_t *error = nullptr;

        // Get xkb state
        cookie = xcb_xkb_get_state(m_conn, XCB_XKB_ID_USE_CORE_KBD);
        reply = xcb_xkb_get_state_reply(m_conn, cookie, &error);

        if (reply) {
            // Set locks state
            d->capslock.enabled = reply->lockedMods & d->capslock.mask;
            d->numlock.enabled  = reply->lockedMods & d->numlock.mask;

            // Set current layout group
            d->group_id = reply->group;

            // Free
            free(reply);
        } else {
            // Log error and disable extension
            qCritical() << "Can't load leds state - " << error->error_code;
            d->enabled = false;
        }
    }

    QString XcbKeyboardBackend::atomName(xcb_get_atom_name_cookie_t cookie) const {
        xcb_get_atom_name_reply_t *reply = nullptr;
        xcb_generic_error_t *error = nullptr;

        // Get atom name
        reply = xcb_get_atom_name_reply(m_conn, cookie, &error);

        QString res;

        if (reply) {
            QByteArray replyText(xcb_get_atom_name_name(reply),
                                 xcb_get_atom_name_name_length(reply));
            res = QString::fromLocal8Bit(replyText);
            free(reply);
        } else {
            // Log error
            qWarning() << "Failed to get atom name: " << error->error_code;
        }
        return res;
    }

    QString XcbKeyboardBackend::atomName(xcb_atom_t atom) const {
        return atomName(xcb_get_atom_name(m_conn, atom));
    }

    uint8_t XcbKeyboardBackend::getIndicatorMask(uint8_t i) const {
        xcb_xkb_get_indicator_map_cookie_t cookie;
        xcb_xkb_get_indicator_map_reply_t *reply = nullptr;
        xcb_generic_error_t *error = nullptr;
        uint8_t mask = 0;

        cookie = xcb_xkb_get_indicator_map(m_conn, XCB_XKB_ID_USE_CORE_KBD, 1 << i);
        reply = xcb_xkb_get_indicator_map_reply(m_conn, cookie, &error);


        if (reply) {
            xcb_xkb_indicator_map_t *map = xcb_xkb_get_indicator_map_maps(reply);

            mask = map->mods;

            free(reply);
        } else {
            // Log error
            qWarning() << "Can't get indicator mask " << error->error_code;
        }
        return mask;
    }

    QList<QString> XcbKeyboardBackend::parseShortNames(QString text) {
        QRegExp re(QStringLiteral(R"(\+([a-z]+)(_vndr\/([a-z]+)\(([a-z]+)\))*)"));
        re.setCaseSensitivity(Qt::CaseInsensitive);

        QList<QString> res;
        QSet<QString> blackList; // blacklist wrong tokens
        blackList << QStringLiteral("inet") << QStringLiteral("group");

        // Loop through matched substrings
        int pos = 0;
        while ((pos = re.indexIn(text, pos)) != -1) {
            if (!blackList.contains(re.cap(1))) {
                if (re.cap(4).length() > 0) {
                    res << re.cap(4);
                } else {
                    res << re.cap(1);
                }
            }
            pos += re.matchedLength();
        }
        return res;
    }

    void XcbKeyboardBackend::dispatchEvents() {
        // Poll events
        while (xcb_generic_event_t *event = xcb_poll_for_event(m_conn)) {
            // Check event types
            qCritical() << "dispatchEvents - rt: " << event->response_type << ", pad0: " << event->pad0;
            if (event->response_type != 0 && event->pad0 == XCB_XKB_STATE_NOTIFY) {
                xcb_xkb_state_notify_event_t *e = (xcb_xkb_state_notify_event_t *)event;

                // Update state
                d->capslock.enabled = e->lockedMods & d->capslock.mask;
                d->numlock.enabled  = e->lockedMods & d->numlock.mask;

                qCritical() << "group " << e->group;
                d->group_id = e->group;
            } else if (event->response_type != 0 && event->pad0 == XCB_XKB_NEW_KEYBOARD_NOTIFY) {
                // Keyboards changed, re-init groups
                initGroups();
            } else if (event->response_type != 0 && event->pad0 == XCB_XKB_MAP_NOTIFY) {
                qCritical() << "MapNotify";
            }
            free(event);
        }
    }

    void XcbKeyboardBackend::connectEventsDispatcher(KeyboardModel *model) {
        // Setup events filter
        xcb_void_cookie_t cookie;
        xcb_xkb_select_events_details_t foo;
        xcb_generic_error_t *error = nullptr;

        cookie = xcb_xkb_select_events(m_conn, XCB_XKB_ID_USE_CORE_KBD,
                XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY, 0,
                XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY, 0, 0, &foo);
        // Check errors
        error = xcb_request_check(m_conn, cookie);
        if (error) {
            qCritical() << "Can't select xck-xkb events: " << error->error_code;
            d->enabled = false;
            return;
        }

        // Flush connection
        xcb_flush(m_conn);

        // Get file descriptor and init socket listener
        int fd = xcb_get_file_descriptor(m_conn);
        m_socket = new QSocketNotifier(fd, QSocketNotifier::Read);

        QObject::connect(m_socket, SIGNAL(activated(int)), model, SLOT(dispatchEvents()));
    }

    void XcbKeyboardBackend::sendLayoutChange() {
        int major, minor, why;
        const int MAX_SYMBOLS_LEN = 100;
        char symbolsList[MAX_SYMBOLS_LEN];

        major = XkbMajorVersion;
        minor = XkbMinorVersion;
        Display *dpy = XkbOpenDisplay(getenv("DISPLAY"), NULL, NULL, &major, &minor, &why);
        if (!dpy) {
            qCritical() << "Can't open display: " << why;
            return;
        }

        switch (d->layout_id) {
            case 0:
                strncpy(symbolsList, d->x11Symbols.toUtf8().data(), MAX_SYMBOLS_LEN);
                break;
            default:
                snprintf(symbolsList, MAX_SYMBOLS_LEN, "pc+planet_vndr/gemini(%s)", ((KeyboardLayout*)d->layouts[d->layout_id])->shortName().toUtf8().data());
        }

        XkbComponentNamesRec cmdNames = {
                .keymap = const_cast<char *>(""),
                .keycodes = const_cast<char *>("evdev"),
                .types = const_cast<char *>("complete"),
                .compat = const_cast<char *>("complete"),
                .symbols = symbolsList,
                .geometry = const_cast<char *>("pc(pc104)")
        };

        XkbDescPtr xkb = XkbGetKeyboardByName(dpy, XCB_XKB_ID_USE_CORE_KBD, &cmdNames,
                                              XkbGBN_AllComponentsMask,
                                              XkbGBN_AllComponentsMask & (~XkbGBN_GeometryMask),
                                              True);
        if (!xkb)
        {
            qCritical() << "Error loading new keyboard description\n";
            return;
        }

    }
}
