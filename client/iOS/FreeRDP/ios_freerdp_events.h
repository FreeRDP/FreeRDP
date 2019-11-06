/*
 RDP event queuing

 Copyright 2013 Thincast Technologies GmbH, Author: Dorian Johnson

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#ifndef IOS_RDP_EVENT_H
#define IOS_RDP_EVENT_H

#import <Foundation/Foundation.h>
#import "ios_freerdp.h"

// For UI: use to send events
BOOL ios_events_send(mfInfo *mfi, NSDictionary *event_description);

// For connection runloop: use to poll for queued input events
BOOL ios_events_get_fds(mfInfo *mfi, void **read_fds, int *read_count, void **write_fds,
                        int *write_count);
BOOL ios_events_check_fds(mfInfo *mfi, fd_set *rfds);
BOOL ios_events_create_pipe(mfInfo *mfi);
void ios_events_free_pipe(mfInfo *mfi);

#endif /* IOS_RDP_EVENT_H */
