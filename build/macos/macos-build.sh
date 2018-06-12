#!/bin/bash
#
# MindForger thinking notebook
#
# Copyright (C) 2016-2018 Martin Dvorak <martin.dvorak@mindforger.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

# make project w/ QtWebEngine instead QtWebKit

echo "IMPORTANT: build mindforger/deps/discount before running this script!"

rm -vrf ../../app/mindforger.app ../../app/mindforger.dmg
cd ../..
make clean
qmake -r -config release mindforger.pro
make -j 8

# cd to directory w/ mindforger.app/ directory
cd app && macdeployqt mindforger.app -dmg -always-overwrite

# eof