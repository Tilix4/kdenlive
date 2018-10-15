#include "test_utils.hpp"

Mlt::Profile reg_profile;
TEST_CASE("Regression")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    // We also mock timeline object to spy few functions and mock others
    TimelineItemModel tim(new Mlt::Profile(), undoStack);
    Mock<TimelineItemModel> timMock(tim);
    TimelineItemModel &tt = timMock.get();
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock);
    TimelineModel::next_id = 0;
    undoStack->undo();
    undoStack->redo();
    undoStack->redo();
    undoStack->undo();
    QString binId0 = createProducer(reg_profile, "red", binModel);
    int c = ClipModel::construct(timeline, binId0, -1, PlaylistState::VideoOnly);
    timeline->m_allClips[c]->m_endlessResize = false;
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->requestClipMove(0, 1, 0));
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->requestItemResize(0, 16, false));
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->requestItemResize(0, 0, false) == -1);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    binModel->clean();
    pCore->m_projectManager = nullptr;
}

TEST_CASE("Regression2")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    // We also mock timeline object to spy few functions and mock others
    TimelineItemModel tim(new Mlt::Profile(), undoStack);
    Mock<TimelineItemModel> timMock(tim);
    TimelineItemModel &tt = timMock.get();
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock);
    TimelineModel::next_id = 0;
    int dummy_id;
    undoStack->undo();
    undoStack->undo();
    undoStack->redo();
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    {
        QString binId0 = createProducer(reg_profile, "red", binModel);
        bool ok = timeline->requestClipInsertion(binId0, 0, 10, dummy_id);
        timeline->m_allClips[dummy_id]->m_endlessResize = false;
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    {
        QString binId0 = createProducer(reg_profile, "red", binModel);
        bool ok = timeline->requestClipInsertion(binId0, 2, 10, dummy_id);
        timeline->m_allClips[3]->m_endlessResize = false;
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(1, 0, 10);
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    undoStack->redo();
    timeline->m_allClips[3]->m_endlessResize = false;
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    {
        REQUIRE(timeline->requestItemResize(3, 0, false) == -1);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    {
        QString binId0 = createProducer(reg_profile, "red", binModel);
        int c = ClipModel::construct(timeline, binId0, -1, PlaylistState::VideoOnly);
        timeline->m_allClips[c]->m_endlessResize = false;
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        REQUIRE(timeline->requestItemResize(3, 15, true) > -1);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(3, 0, 0);
        REQUIRE_FALSE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        REQUIRE(timeline->requestItemResize(3, 16, false) == -1);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        REQUIRE(timeline->requestItemResize(3, 16, true) > -1);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        QString binId0 = createProducer(reg_profile, "red", binModel);
        bool ok = timeline->requestClipInsertion(binId0, 0, 1, dummy_id);
        REQUIRE_FALSE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->redo();
    binModel->clean();
    pCore->m_projectManager = nullptr;
}

/*
TEST_CASE("Regression 3")
{
    Mlt::Profile profile;
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);
    TimelineModel::next_id = 0;
    int dummy_id;
    std::shared_ptr<Mlt::Producer> producer0 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer0->set("length", 20);
    producer0->set("out", 19);
    ClipModel::construct(timeline, producer0 );
    {
        bool ok = timeline->requestTrackInsertion(-1, dummy_id);
        REQUIRE(ok);
    }
    TrackModel::construct(timeline);
    TrackModel::construct(timeline);
    std::shared_ptr<Mlt::Producer> producer1 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer1->set("length", 20);
    producer1->set("out", 19);
    ClipModel::construct(timeline, producer1 );
    std::shared_ptr<Mlt::Producer> producer2 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer2->set("length", 20);
    producer2->set("out", 19);
    ClipModel::construct(timeline, producer2 );
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    std::shared_ptr<Mlt::Producer> producer3 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer3->set("length", 20);
    producer3->set("out", 19);
    ClipModel::construct(timeline, producer3 );
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    std::shared_ptr<Mlt::Producer> producer4 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer4->set("length", 20);
    producer4->set("out", 19);
    ClipModel::construct(timeline, producer4 );
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    std::shared_ptr<Mlt::Producer> producer5 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer5->set("length", 20);
    producer5->set("out", 19);
    ClipModel::construct(timeline, producer5 );
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    std::shared_ptr<Mlt::Producer> producer6 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer6->set("length", 20);
    producer6->set("out", 19);
    ClipModel::construct(timeline, producer6 );
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(0,1 ,10 );
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(4,2 ,12 );
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    {
        auto group = {4, 0};
        bool ok = timeline->requestClipsGroup(group);
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(4,1 ,10 );
        REQUIRE_FALSE(ok);
    }
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(4,1 ,100 );
        REQUIRE_FALSE(ok);
    }
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(0,3 ,100 );
        REQUIRE(ok);
    }
    std::shared_ptr<Mlt::Producer> producer7 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer7->set("length", 20);
    producer7->set("out", 19);
    ClipModel::construct(timeline, producer7 );
    {
        bool ok = timeline->requestTrackInsertion(-1, dummy_id);
        REQUIRE(ok);
    }
    undoStack->undo();
    {
        bool ok = timeline->requestClipMove(0,1 ,5 );
        REQUIRE(ok);
    }
    {
        bool ok = timeline->requestTrackDeletion(1);
        REQUIRE(ok);
    }
}

TEST_CASE("Regression 4")
{
    Mlt::Profile profile;
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);
    TimelineModel::next_id = 0;
    int dummy_id;
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    {
        std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile, "color", "red");
        producer->set("length", 62);
        producer->set("out", 61);
        timeline->requestClipInsertion(producer,10 ,453, dummy_id );
    }
    timeline->requestClipMove(11,10 ,453, true, true );
    {
        std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile, "color", "red");
        producer->set("length", 62);
        producer->set("out", 61);
        timeline->requestClipInsertion(producer,9 ,590, dummy_id );
    }
    timeline->requestItemResize(11,62 ,true, false, true );
    timeline->requestItemResize(11,62 ,true, true, true );
    timeline->requestClipMove(11,10 ,507, true, true );
    timeline->requestClipMove(12,10 ,583, false, false );
    timeline->requestClipMove(12,9 ,521, true, true );
}
*/
