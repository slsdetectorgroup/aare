****************
Workflow
****************

This page describes how we develop aare. 

GitHub centric
~~~~~~~~~~~~~~~~~~

We use GitHub for all development. Issues and pull requests provide a platform for collaboration as well
as a record of the development process. Even if we discuss things in person, we record the outcome in an issue.
If a particular implementation is chosen over another, the reason should be recorded in the pull request.


Branches
~~~~~~~~~~~~~~~~~~

We aim for an as lightweight branching strategy as possible. Short-lived feature branches are merged back into main. 
The main branch is expected to always be in a releasable state. A release is simply a tag on main which provides a
reference and triggers the CI to build the release artifacts (conda, pypi etc.). For large features consider merging
smaller chunks into main as they are completed, rather than waiting for the entire feature to be finished. Worst case 
make sure your feature branch merges with main regularly to avoid large merge conflicts later on.


Releases
~~~~~~~~~~~~~~~~~~

Release early, release often. As soon as "enough" new features have been implemented, a release is created.
A release should not be a big thing, rather a routine part of development that does not require any special person or
unfamiliar steps.



Checklists
~~~~~~~~~~~~~~~~~~

Feature: 

#. Create a new issue for the feature (label feature)
#. Create a new branch from main. 
#. Implement the feature including test and documentation.
#. Create a pull request linked to the issue.
#. Code is reviewed by at least one other person
#. Once approved, the branch is merged into main


BugFix:

Essentially the same as for a feature, if possible start with
a failing test that demonstrates the bug.

#. Create a new issue for the bug (label bug)
#. Create a new branch from main.
#. **Write a test that fails for the bug**
#. Implement the fix
#. **Run the test to ensure it passes**
#. Create a pull request linked to the issue.
#. Code is reviewed by at least one other person
#. Once approved, the branch is merged into main

Release:

#. Once "enough" new features have been implemented, a release is created.
#. Create the release in GitHub describing the new features and bug fixes.
#. CI makes magic.