
### Creating a Unit

##### Naming the unit.

Pick a name that describes what the unit does.  Longer names are
better, as they are less likely to conflict with other units and with
user defined names.

##### Document the Unit

The unit doesn't serve much purpose if the user can't figure out how
to use it.
If the unit does not automatically execute when included, specify the
usage for the unit.
Specify what variables are set.  If a variable is set, make
sure it is always set, unless the variable's existence is predicated
on another variable.  e.g. a package has \_package\_FOUND, and any other
package\_\* variables only exist if \_package\_FOUND is set to true.

##### Internal Documentation

Document the processing steps that the unit takes.  It is important to
document why each step is done.

##### Unit Scope

Wrap the entire unit in its own scope.
This ensures that any local variables are kept separate from the
user's namespace.

    {
      # unit function definition
      ...
    }

##### Auto-Run

When possible run the unit when it is loaded.

    {
      function myunit {
        print 'hello world';
      }

      myunit;
    }

##### Setting Variables

Variables that are set should be named appropriately so that they are
unique and unlikely to conflict with MKC variables, other unit
variables and user variables.

Unless a variable is going to be set for the user, make it a local
variable so it doesn't pollute the namespace.

    {
      set local mycheck true;

      function myunit {
        set local hw 'hello-world';
        print ${hw};
      }

      myunit;
    }

##### Marking Variables for Output

Many variables do not need to be saved in the project's configuration
file.  Mark the variables to disable or enable the output as
appropriate.

    {
      function myunit {
        if (is_defined (${myunit_processed)) {
          return;
        }

        set _function_myunit_lib_func true;
        set myunit_processed true;
        mark_variable myunit_processed disable-output;
        mark_variable _function_myunit_lib_func enable-output;
      }

      myunit;
    }

