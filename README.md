GDALWRAP
========

*C++11 GDAL wrapper*

* http://gdal.org
* http://www.openrobots.org/wiki


INSTALL
-------

    git clone https://github.com/pierriko/gdalwrap.git && cd gdalwrap
    mkdir build && cd build
    cmake -DCMAKE_INSTALL_PREFIX=$HOME/devel ..
    make -j8 && make install


CONTRIBUTE
----------

Code is available on GitHub at https://github.com/pierriko/gdalwrap

Feel free to fork, pull request or submit issues to improve the project!

* https://github.com/pierriko/gdalwrap/fork
* https://github.com/pierriko/gdalwrap/issues
* https://github.com/pierriko/gdalwrap/pulls
* https://help.github.com/articles/fork-a-repo
* https://help.github.com/articles/using-pull-requests

### STYLE

Please configure your editor to insert 4 spaces instead of TABs, maximum line
length to 79, `lower_case_with_underscores` instead of `CamelCase`. Most of the
rules are taken from [Python PEP8](http://www.python.org/dev/peps/pep-0008/)

Other ideas can be found in Google Guides:
[Python](http://google-styleguide.googlecode.com/svn/trunk/pyguide.html),
[C++](http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml).


LICENSE
-------

[BSD 3-Clause](http://opensource.org/licenses/BSD-3-Clause)

Copyright © 2013 CNRS-LAAS
