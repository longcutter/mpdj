// MPDJ (touch scrolling, 2026-11-17): a list view that scrolls under the finger without
// moving the selection (Mike: the box moved against the finger when the drag moved the
// selection). Shared by cList and cParamValueList, which keep the same fields.
#pragma once

// a drag of rows rows (+ = later rows come up): the view moves, the selection stays
template <class C> void mpdjListScroll(C* c, int rows)
{
    if (!c->list)
        return;
    int maxTop = c->list->length > c->list->linesCount ? c->list->length - c->list->linesCount : 0;
    if (c->mpdjViewTop < 0)
    {
        c->mpdjViewTop = (int16_t)c->textListPos;
        c->mpdjViewSel = c->list->start;
    }
    int top = c->mpdjViewTop + rows;
    c->mpdjViewTop = (int16_t)(top < 0 ? 0 : (top > maxTop ? maxTop : top));
}

// at the start of update(): the scrolled view, if any. True = the selection is outside
// the view (draw no box). A selection change that lands outside the view ends it (the
// list goes back to following the selection).
template <class C> bool mpdjListView(C* c)
{
    if (c->mpdjViewTop < 0 || !c->list)
        return false;
    // setValue() sets the new selection as listPosition and update() animates list->start
    // towards it; in a scrolled view jump straight there
    if (c->listPosition != c->list->start && c->listPosition >= 0 && c->listPosition < c->list->length)
        c->list->start = c->listPosition;
    if (c->list->start != c->mpdjViewSel)
    {
        c->mpdjViewSel = c->list->start;
        if (c->list->start < c->mpdjViewTop || c->list->start >= c->mpdjViewTop + c->list->linesCount)
        {
            c->mpdjViewTop = -1;
            c->setData(c->list);
            return false;
        }
    }
    int maxTop = c->list->length > c->list->linesCount ? c->list->length - c->list->linesCount : 0;
    if (c->mpdjViewTop > maxTop)
        c->mpdjViewTop = (int16_t)maxTop;
    c->textListPos = (uint16_t)c->mpdjViewTop;
    int bar = c->list->start - c->mpdjViewTop;
    if (bar < 0 || bar >= c->list->linesCount)
        return true;
    c->barPos = (uint16_t)bar;
    return false;
}
