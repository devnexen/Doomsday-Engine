/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_TEXTVALUE_H
#define LIBCORE_TEXTVALUE_H

#include "../Value"
#include "../String"

#include <list>

namespace de {

class CString;

/**
 * The TextValue class is a subclass of Value that holds a text string.
 *
 * @ingroup data
 */
class DE_PUBLIC TextValue : public Value
{
public:
    /// An error occurs in string pattern replacements. @ingroup errors
    DE_ERROR(IllegalPatternError);

public:
    TextValue(String const &initialValue = "");

    ~TextValue();

    /// Converts the TextValue to plain text.
    operator String const &() const;

    operator CString() const;

    Text typeId() const;
    Value *duplicate() const;
    Number asNumber() const;
    Text asText() const;
    Record *memberScope() const;
    dsize size() const;
    bool contains(Value const &value) const;
    Value *duplicateElement(const Value &charPos) const;
    Value *next();
    bool isTrue() const;
    dint compare(Value const &value) const;
    void sum(Value const &value);
    void multiply(Value const &value);
    void divide(Value const &value);
    void modulo(Value const &divisor);

    static String substitutePlaceholders(String const &pattern, const std::list<Value const *> &args);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

protected:
    /// Changes the text of the value.
    void setValue(String const &text);

private:
    Text _value;
    String::const_iterator *_iteration;
};

} // namespace de

#endif /* LIBCORE_TEXTVALUE_H */
