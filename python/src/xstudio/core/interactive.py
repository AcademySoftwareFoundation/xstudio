# SPDX-License-Identifier: Apache-2.0
import sys
from code import InteractiveInterpreter


class InteractiveSession(InteractiveInterpreter):
    """Closely emulate the behavior of the interactive Python interpreter.
    This class builds on InteractiveInterpreter and adds prompting
    using the familiar sys.ps1 and sys.ps2, and input buffering.
    """

    def __init__(self, locals=None, filename="<console>"):
        """Constructor.
        The optional locals argument will be passed to the
        InteractiveInterpreter base class.
        The optional filename argument should specify the (file)name
        of the input stream; it will show up in tracebacks.
        """
        InteractiveInterpreter.__init__(self, locals)
        self.filename = filename
        self.resetbuffer()
        self.wants_input = False
        self.needs_more = 0
        self.interacting = False
        self.exitmsg = None

    def resetbuffer(self):
        """Reset the input buffer."""
        self.buffer = []

    def keyboard_interrupt(self):
        self.write("\nKeyboardInterrupt\n")
        self.resetbuffer()
        self.needs_more = 0
        self.wants_input = False
        self.interact_more()

    def interact_more(self, input_str=None):
        if self.interacting:
            if self.wants_input and input_str is not None:
                self.wants_input = False
                self.needs_more = self.push(input_str)

            if self.needs_more:
                prompt = sys.ps2
            else:
                prompt = sys.ps1

            try:
                self.raw_input(prompt)
            except EOFError:
                self.write("\n")
                # can't happen...
        else:
            if self.exitmsg is None:
                self.write('now exiting %s...\n' % self.__class__.__name__)
            elif self.exitmsg != '':
                self.write('%s\n' % exitmsg)


    def interact(self, banner, exitmsg=None):
        """Closely emulate the interactive Python console.
        The optional banner argument specifies the banner to print
        before the first interaction; by default it prints a banner
        similar to the one printed by the real Python interpreter,
        followed by the current class name in parentheses (so as not
        to confuse this with the real interpreter -- since it's so
        close!).
        The optional exitmsg argument specifies the exit message
        printed when exiting. Pass the empty string to suppress
        printing an exit message. If exitmsg is not given or None,
        a default message is printed.
        """
        try:
            sys.ps1
        except AttributeError:
            sys.ps1 = ">>> "
        try:
            sys.ps2
        except AttributeError:
            sys.ps2 = "... "
        self.write("%s - console\nUse XSTUDIO object for api access.\n" % (banner))
        # cprt = 'Type "help", "copyright", "credits" or "license" for more information.'
        # self.write("%s Python %s on %s\n%s\n(%s)\n" %
        #            (banner, sys.version, sys.platform, cprt,
        #             self.__class__.__name__))

        self.exitmsg = exitmsg
        self.interacting = True
        self.needs_more = 0
        self.interact_more()

    def push(self, line):
        """Push a line to the interpreter.
        The line should not have a trailing newline; it may have
        internal newlines.  The line is appended to a buffer and the
        interpreter's runsource() method is called with the
        concatenated contents of the buffer as source.  If this
        indicates that the command was executed or invalid, the buffer
        is reset; otherwise, the command is incomplete, and the buffer
        is left as it was after the line was appended.  The return
        value is 1 if more input is required, 0 if the line was dealt
        with in some way (this is the same as runsource()).
        """
        self.buffer.append(line)
        source = "\n".join(self.buffer)
        more = self.runsource(source, self.filename)
        if not more:
            self.resetbuffer()
        return more

    def raw_input(self, prompt=""):
        """Write a prompt and read a line.
        The returned line does not include the trailing newline.
        When the user enters the EOF key sequence, EOFError is raised.
        The base implementation uses the built-in function
        input(); a subclass may replace this with a different
        implementation.
        """
        self.write(prompt)
        self.wants_input = True