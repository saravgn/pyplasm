/*
  ==============================================================================

   This file is part of the JUCE library - "Jules' Utility Class Extensions"
   Copyright 2004-11 by Raw Material Software Ltd.

  ------------------------------------------------------------------------------

   JUCE can be redistributed and/or modified under the terms of the GNU General
   Public License (Version 2), as published by the Free Software Foundation.
   A copy of the license is included in the JUCE distribution, or can be found
   online at www.gnu.org/licenses.

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

  ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.rawmaterialsoftware.com/juce for more information.

  ==============================================================================
*/

MidiMessageSequence::MidiMessageSequence()
{
}

MidiMessageSequence::MidiMessageSequence (const MidiMessageSequence& other)
{
    list.ensureStorageAllocated (other.list.size());

    for (int i = 0; i < other.list.size(); ++i)
        list.add (new MidiEventHolder (other.list.getUnchecked(i)->message));
}

MidiMessageSequence& MidiMessageSequence::operator= (const MidiMessageSequence& other)
{
    MidiMessageSequence otherCopy (other);
    swapWith (otherCopy);
    return *this;
}

void MidiMessageSequence::swapWith (MidiMessageSequence& other) noexcept
{
    list.swapWithArray (other.list);
}

MidiMessageSequence::~MidiMessageSequence()
{
}

void MidiMessageSequence::clear()
{
    list.clear();
}

int MidiMessageSequence::getNumEvents() const
{
    return list.size();
}

MidiMessageSequence::MidiEventHolder* MidiMessageSequence::getEventPointer (const int index) const
{
    return list [index];
}

double MidiMessageSequence::getTimeOfMatchingKeyUp (const int index) const
{
    const MidiEventHolder* const meh = list [index];

    if (meh != nullptr && meh->noteOffObject != nullptr)
        return meh->noteOffObject->message.getTimeStamp();
    else
        return 0.0;
}

int MidiMessageSequence::getIndexOfMatchingKeyUp (const int index) const
{
    const MidiEventHolder* const meh = list [index];

    return meh != nullptr ? list.indexOf (meh->noteOffObject) : -1;
}

int MidiMessageSequence::getIndexOf (MidiEventHolder* const event) const
{
    return list.indexOf (event);
}

int MidiMessageSequence::getNextIndexAtTime (const double timeStamp) const
{
    const int numEvents = list.size();

    int i;
    for (i = 0; i < numEvents; ++i)
        if (list.getUnchecked(i)->message.getTimeStamp() >= timeStamp)
            break;

    return i;
}

//==============================================================================
double MidiMessageSequence::getStartTime() const
{
    return getEventTime (0);
}

double MidiMessageSequence::getEndTime() const
{
    return getEventTime (list.size() - 1);
}

double MidiMessageSequence::getEventTime (const int index) const
{
    const MidiEventHolder* const e = list [index];
    return e != nullptr ? e->message.getTimeStamp() : 0.0;
}

//==============================================================================
void MidiMessageSequence::addEvent (const MidiMessage& newMessage,
                                    double timeAdjustment)
{
    MidiEventHolder* const newOne = new MidiEventHolder (newMessage);

    timeAdjustment += newMessage.getTimeStamp();
    newOne->message.setTimeStamp (timeAdjustment);

    int i;
    for (i = list.size(); --i >= 0;)
        if (list.getUnchecked(i)->message.getTimeStamp() <= timeAdjustment)
            break;

    list.insert (i + 1, newOne);
}

void MidiMessageSequence::deleteEvent (const int index,
                                       const bool deleteMatchingNoteUp)
{
    if (isPositiveAndBelow (index, list.size()))
    {
        if (deleteMatchingNoteUp)
            deleteEvent (getIndexOfMatchingKeyUp (index), false);

        list.remove (index);
    }
}

struct MidiMessageSequenceSorter
{
    static int compareElements (const MidiMessageSequence::MidiEventHolder* const first,
                                const MidiMessageSequence::MidiEventHolder* const second) noexcept
    {
        const double diff = first->message.getTimeStamp() - second->message.getTimeStamp();
        return (diff > 0) - (diff < 0);
    }
};

void MidiMessageSequence::addSequence (const MidiMessageSequence& other,
                                       double timeAdjustment,
                                       double firstAllowableTime,
                                       double endOfAllowableDestTimes)
{
    firstAllowableTime -= timeAdjustment;
    endOfAllowableDestTimes -= timeAdjustment;

    for (int i = 0; i < other.list.size(); ++i)
    {
        const MidiMessage& m = other.list.getUnchecked(i)->message;
        const double t = m.getTimeStamp();

        if (t >= firstAllowableTime && t < endOfAllowableDestTimes)
        {
            MidiEventHolder* const newOne = new MidiEventHolder (m);
            newOne->message.setTimeStamp (timeAdjustment + t);

            list.add (newOne);
        }
    }

    MidiMessageSequenceSorter sorter;
    list.sort (sorter, true);
}

//==============================================================================
void MidiMessageSequence::updateMatchedPairs()
{
    for (int i = 0; i < list.size(); ++i)
    {
        const MidiMessage& m1 = list.getUnchecked(i)->message;

        if (m1.isNoteOn())
        {
            list.getUnchecked(i)->noteOffObject = nullptr;
            const int note = m1.getNoteNumber();
            const int chan = m1.getChannel();
            const int len = list.size();

            for (int j = i + 1; j < len; ++j)
            {
                const MidiMessage& m = list.getUnchecked(j)->message;

                if (m.getNoteNumber() == note && m.getChannel() == chan)
                {
                    if (m.isNoteOff())
                    {
                        list.getUnchecked(i)->noteOffObject = list[j];
                        break;
                    }
                    else if (m.isNoteOn())
                    {
                        list.insert (j, new MidiEventHolder (MidiMessage::noteOff (chan, note)));
                        list.getUnchecked(j)->message.setTimeStamp (m.getTimeStamp());
                        list.getUnchecked(i)->noteOffObject = list[j];
                        break;
                    }
                }
            }
        }
    }
}

void MidiMessageSequence::addTimeToMessages (const double delta)
{
    for (int i = list.size(); --i >= 0;)
        list.getUnchecked (i)->message.setTimeStamp (list.getUnchecked (i)->message.getTimeStamp()
                                                      + delta);
}

//==============================================================================
void MidiMessageSequence::extractMidiChannelMessages (const int channelNumberToExtract,
                                                      MidiMessageSequence& destSequence,
                                                      const bool alsoIncludeMetaEvents) const
{
    for (int i = 0; i < list.size(); ++i)
    {
        const MidiMessage& mm = list.getUnchecked(i)->message;

        if (mm.isForChannel (channelNumberToExtract)
             || (alsoIncludeMetaEvents && mm.isMetaEvent()))
        {
            destSequence.addEvent (mm);
        }
    }
}

void MidiMessageSequence::extractSysExMessages (MidiMessageSequence& destSequence) const
{
    for (int i = 0; i < list.size(); ++i)
    {
        const MidiMessage& mm = list.getUnchecked(i)->message;

        if (mm.isSysEx())
            destSequence.addEvent (mm);
    }
}

void MidiMessageSequence::deleteMidiChannelMessages (const int channelNumberToRemove)
{
    for (int i = list.size(); --i >= 0;)
        if (list.getUnchecked(i)->message.isForChannel (channelNumberToRemove))
            list.remove(i);
}

void MidiMessageSequence::deleteSysExMessages()
{
    for (int i = list.size(); --i >= 0;)
        if (list.getUnchecked(i)->message.isSysEx())
            list.remove(i);
}

//==============================================================================
void MidiMessageSequence::createControllerUpdatesForTime (const int channelNumber,
                                                          const double time,
                                                          OwnedArray<MidiMessage>& dest)
{
    bool doneProg = false;
    bool donePitchWheel = false;
    Array <int> doneControllers;
    doneControllers.ensureStorageAllocated (32);

    for (int i = list.size(); --i >= 0;)
    {
        const MidiMessage& mm = list.getUnchecked(i)->message;

        if (mm.isForChannel (channelNumber)
             && mm.getTimeStamp() <= time)
        {
            if (mm.isProgramChange())
            {
                if (! doneProg)
                {
                    dest.add (new MidiMessage (mm, 0.0));
                    doneProg = true;
                }
            }
            else if (mm.isController())
            {
                if (! doneControllers.contains (mm.getControllerNumber()))
                {
                    dest.add (new MidiMessage (mm, 0.0));
                    doneControllers.add (mm.getControllerNumber());
                }
            }
            else if (mm.isPitchWheel())
            {
                if (! donePitchWheel)
                {
                    dest.add (new MidiMessage (mm, 0.0));
                    donePitchWheel = true;
                }
            }
        }
    }
}


//==============================================================================
MidiMessageSequence::MidiEventHolder::MidiEventHolder (const MidiMessage& message_)
   : message (message_),
     noteOffObject (nullptr)
{
}

MidiMessageSequence::MidiEventHolder::~MidiEventHolder()
{
}
