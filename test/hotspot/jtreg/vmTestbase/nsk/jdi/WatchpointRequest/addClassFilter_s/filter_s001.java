/*
 * Copyright (c) 2001, 2024, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package nsk.jdi.WatchpointRequest.addClassFilter_s;

import nsk.share.*;
import nsk.share.jpda.*;
import nsk.share.jdi.*;

import com.sun.jdi.*;
import com.sun.jdi.event.*;
import com.sun.jdi.request.*;

import java.util.*;
import java.io.*;

/**
 * The test for the implementation of an object of the type
 * WatchpointRequest.
 *
 * The test checks that results of the method
 * <code>com.sun.jdi.WatchpointRequest.addClassFilter(String)</code>
 * complies with its spec.
 *
 * The test checks up on the following assertion:
 *    Restricts the events generated by this request to those
 *    whose location is in a class
 *    whose name matches a restricted regular expression.
 * The cases to check include both a pattern that begin with '*' and
 * one that end with '*'.
 * The cases to test include AccessWatchpointRequest.
 *
 * The test works as follows.
 * - The debugger
 *   - sets up two WatchpointRequests,
 *   - using patterns that begins with '*' and ends with *,
 *     restricts the Requests, so that,
 *     first time events will be filtered only from thread1 and
 *     second time events will be filtered only from thread2,
 *   - resumes the debuggee, and
 *   - waits for expected WatchpointEvents.
 * - The debuggee creates and starts two threads, thread1 and thread2,
 *   that being run, generate Events to test the filters.
 * - Upon getting the events, the debugger performs checks required.
 */

public class filter_s001 extends TestDebuggerType1 {

    public static void main (String argv[]) {
        int result = run(argv,System.out);
        if (result != 0) {
            throw new RuntimeException("TEST FAILED with result " + result);
        }
    }

    public static int run (String argv[], PrintStream out) {
        debuggeeName = "nsk.jdi.WatchpointRequest.addClassFilter_s.filter_s001a";
        return new filter_s001().runThis(argv, out);
    }

    private String testedClassName1 = "*TestClass11";

    private String testedClassName2 =
      "nsk.jdi.WatchpointRequest.addClassFilter_s.Thread2filter_s001a*";

    String className1 = "nsk.jdi.WatchpointRequest.addClassFilter_s.TestClass10";
    String className2 = "nsk.jdi.WatchpointRequest.addClassFilter_s.TestClass20";

    protected void testRun() {

        if ( !vm.canWatchFieldAccess() ) {
            display("......vm.canWatchFieldAccess == false :: test cancelled");
            vm.exit(Consts.JCK_STATUS_BASE);
            return;
        }

        EventRequest  eventRequest1 = null;
        EventRequest  eventRequest2 = null;

        String        property1     = "AccessWatchpointRequest1";
        String        property2     = "AccessWatchpointRequest2";

        ThreadReference thread1 = null;
        ThreadReference thread2 = null;

        String thread1Name = "thread1";
        String thread2Name = "thread2";

        String fieldName1 = "var101";
        String fieldName2 = "var201";

        ReferenceType testClassReference = null;
        Event newEvent = null;

        for (int i = 0; ; i++) {

            if (!shouldRunAfterBreakpoint()) {
                vm.resume();
                break;
            }

            display(":::::: case: # " + i);

            switch (i) {

                case 0:
                    testClassReference =
                         (ReferenceType) vm.classesByName(className1).get(0);

                    eventRequest1 = setting21AccessWatchpointRequest (null,
                                           testClassReference, fieldName1,
                                           EventRequest.SUSPEND_ALL, property1);

                    ((AccessWatchpointRequest) eventRequest1).addClassFilter(testedClassName1);

                    display("......waiting for AccessWatchpointEvent in tested thread");
                    newEvent = eventHandler.waitForRequestedEvent(new EventRequest[]{eventRequest1}, waitTime, true);

                    if ( !(newEvent instanceof AccessWatchpointEvent)) {
                        setFailedStatus("ERROR: new event is not AccessWatchpointEvent");
                    } else {
                        String property = (String) newEvent.request().getProperty("number");
                        display("       got new AccessWatchpointEvent with property 'number' == " + property);

                        if ( !property.equals(property1) ) {
                            setFailedStatus("ERROR: property is not : " + property1);
                        }
                    }
                vm.resume();
                    break;

                case 1:
                    testClassReference =
                         (ReferenceType) vm.classesByName(className2).get(0);

                    eventRequest2 = setting21AccessWatchpointRequest (null,
                                           testClassReference, fieldName2,
                                           EventRequest.SUSPEND_ALL, property2);

                    ((AccessWatchpointRequest) eventRequest2).addClassFilter(testedClassName2);

                    display("......waiting for AccessWatchpointEvent in tested thread");
                    newEvent = eventHandler.waitForRequestedEvent(new EventRequest[]{eventRequest2}, waitTime, true);

                    if ( !(newEvent instanceof AccessWatchpointEvent)) {
                        setFailedStatus("ERROR: new event is not AccessWatchpointEvent");
                    } else {
                        String property = (String) newEvent.request().getProperty("number");
                        display("       got new AccessWatchpointEvent with property 'number' == " + property);

                        if ( !property.equals(property2) ) {
                            setFailedStatus("ERROR: property is not : " + property2);
                        }
                    }
                vm.resume();
                    break;
                default:
                    throw new Failure("** default case 2 **");
            }
        }
        return;
    }

    private AccessWatchpointRequest setting21AccessWatchpointRequest (
                                                  ThreadReference thread,
                                                  ReferenceType   fieldClass,
                                                  String          fieldName,
                                                  int             suspendPolicy,
                                                  String          property        ) {
        try {
            display("......setting up AccessWatchpointRequest:");
            display("       thread: " + thread + "; fieldClass: " + fieldClass + "; fieldName: " + fieldName);
            Field field = fieldClass.fieldByName(fieldName);

            AccessWatchpointRequest
            awr = eventRManager.createAccessWatchpointRequest(field);
            awr.putProperty("number", property);

            if (thread != null)
                awr.addThreadFilter(thread);
            awr.setSuspendPolicy(suspendPolicy);

            display("      AccessWatchpointRequest has been set up");
            return awr;
        } catch ( Exception e ) {
            throw new Failure("** FAILURE to set up AccessWatchpointRequest **");
        }
    }

}
