#!/usr/bin/env python
# SPDX-License-Identifier: Apache-2.0

import sys
import argparse

class LogLine():
    def __init__(self, line):
        self.line = line
        [self.something, self.module, self.level, self.actor, self.timestamp, self.action, self.detail] = self.line.split("|", 6)

        self.id = ""
        self.make = False
        self.unmake = False
        if self.action == "on_cleanup":
            self.id = self.detail.split(" ; ", 3)[1].strip()
            self.unmake = True
        elif self.action.startswith("make_actor"):
            self.id = self.detail.split(" ; ", 3)[1].strip()
            self.make = True

# 9|caf_flow|DEBUG|actor0|139866115874624|make_actor<caf::io::basp_broker>|/tmp/bobscratch/build/libcaf_core/caf/make_actor.hpp:27|SPAWN ; ID = 4 ; NAME = caf.system.basp-broker ; TYPE = caf.io.basp_broker ; ARGS = [actor_config(hidden_flag)] ; NODE = 55844C7CC2C19DFBB296A94ED6107BF430CE3A3E#3678201


def hung_main():
    """Do main function."""
    retval = 0
    parser = argparse.ArgumentParser(
        # description=__doc__ + " Scans CAF logs.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )

    parser.add_argument(
        "-l", "--log", action="store",
        type=str,
        help="Log file",
        default=None
    )

    args = parser.parse_args()

# 468 caf_flow DEBUG actor12 140372996126464 make_actor<caf::{anonymous}::impl> /tmp/bobscratch/build/libcaf_core/caf/make_actor.hpp:27 SPAWN ; ID = 13 ; NAME = scoped_actor ; TYPE = caf.ANON.impl ; ARGS = [actor_config(detached_flag)] ; NODE = 999146CCA9C0736D297B32D1725433D9877D838B#3673267
# 468 caf_flow DEBUG actor13 140372996126464 on_cleanup /tmp/bobscratch/build/libcaf_core/caf/local_actor.cpp:172 TERMINATE ; ID = 13 ; REASON = none ; NODE = 999146CCA9C0736D297B32D1725433D9877D838B#3673267


    if args.log:
        logs = []

        with open(args.log) as logfile:
            for line in logfile:
                line = line.rstrip()
                if line[0].isdigit():
                    try:
                        logs.append(LogLine(line))
                    except:
                        pass

        # prune logs we don't want.

        logs = [x for x in logs if x.module == "caf_flow"]
        logs = [x for x in logs if x.action == "on_cleanup" or x.action.startswith("make_actor")]

        # for l in logs:
        #     print(l.line)

        # scan logs handling actors..

        active = {}
        created_by = {}
        creation = {}

        for l in logs:
            if l.make:
                if l.id in active:
                    print(l.id)
                else:
                    active[l.id] = l
                    creation[l.id] = l
                    if l.id not in created_by:
                        created_by[l.id] = "ID = "+l.actor[5:]

            elif l.unmake:
                if l.id not in active:
                    print(l.line)
                else:
                    del active[l.id]

        for a in active:
            if created_by[active[a].id] in creation:
                print(active[a].line)
                print(creation[created_by[active[a].id]].detail+"\n")
            else:
                print(active[a].line)

if __name__ == "__main__":
    hung_main()
