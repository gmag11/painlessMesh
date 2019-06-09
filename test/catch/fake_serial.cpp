/*
  DSM2_tx implements the serial communication protocol used for operating
  the RF modules that can be found in many DSM2-compatible transmitters.
  Copyrigt (C) 2012  Erik Elmore <erik@ironsavior.net>
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstring>
#include <iomanip>
#include <iostream>

#include "fake_serial.hpp"

void FakeSerial::begin(unsigned long speed) { return; }

void FakeSerial::end() { return; }

size_t FakeSerial::write(const unsigned char buf[], size_t size) {
  using namespace std;
  ios_base::fmtflags oldFlags = cout.flags();
  streamsize oldPrec = cout.precision();
  char oldFill = cout.fill();

  cout << "Serial::write: ";
  cout << internal << setfill('0');

  for (unsigned int i = 0; i < size; i++) {
    cout << setw(2) << hex << (unsigned int)buf[i] << " ";
  }
  cout << endl;

  cout.flags(oldFlags);
  cout.precision(oldPrec);
  cout.fill(oldFill);

  return size;
}

void FakeSerial::print(const char* buf) { std::cout << buf; }

void FakeSerial::println() { std::cout << std::endl; }

FakeSerial Serial;
