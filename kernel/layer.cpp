#include  <algorithm>
#include "console.hpp"
#include "logger.hpp"
#include "layer.hpp"

Layer::Layer(unsigned int id) : id_{id} {
};

unsigned int Layer::ID() const {
    return id_;
}

Layer& Layer::SetWindow(const std::shared_ptr<Window>& window) {
    window_ = window;
    return *this;
}

Layer& Layer::Move(Vector2D<int> pos) {
    pos_ = pos;
    return *this;
}

Layer& Layer::MoveRelative(Vector2D<int> pos_diff) {
    pos_ += pos_diff;
    return *this;
}

void Layer::DrawTo(FrameBuffer& screen, const Rectangle<int>& area) const {
    if(window_) {
        window_->DrawTo(screen, pos_, area);
    }
}

void LayerManager::SetWriter(FrameBuffer* screen) {
    screen_ = screen;
    FrameBufferConfig back_config_ = screen->Config();
    back_config_.frame_buffer = nullptr;
    back_buffer_.Initialize(back_config_);
}

Layer& LayerManager::NewLayer() {
    ++latest_id_;
    layers_.emplace_back(new Layer{latest_id_}); // return void type ?
    return *layers_.back();
}

Layer* LayerManager::FindLayer(unsigned int id) {
    auto pred = [id](const std::unique_ptr<Layer>& elem) {
        return elem->ID() == id;
    };
    auto it = std::find_if(layers_.begin(), layers_.end(), pred);
    if(it == layers_.end()) {
        return nullptr;
    }
    return it->get();
}

Vector2D<int> Layer::GetPosition() const {
    return pos_;
}

Layer& Layer::SetDraggable(bool draggable) {
    draggable_ = draggable;
    return *this;
} 

bool Layer::IsDraggable() const {
    return draggable_;
}

void LayerManager::Move(unsigned int id, Vector2D<int> new_pos) {
    auto layer =  FindLayer(id);
    const auto window_size = layer->GetWindow()->Size();
    const auto old_pos = layer->GetPosition();
    layer->Move(new_pos);
    Draw({old_pos, window_size}); // 古い位置を再描画
    Draw(id); // 新しい位置を再描画
}

void LayerManager::MoveRelative(unsigned int id, Vector2D<int> pos_diff) {
    auto layer = FindLayer(id);
    const auto window_size = layer->GetWindow()->Size();
    const auto old_pos = layer->GetPosition();
    layer->MoveRelative(pos_diff);
    Draw({old_pos, window_size}); 
    Draw(id); 
}

void LayerManager::Draw(const Rectangle<int>& area) const {
    // back bufferに全ての書き込みが終わってから、frame bufferにコピー
    for(auto layer : layer_stack_) {
        layer->DrawTo(back_buffer_, area);
    }
    screen_->Copy(area.pos, back_buffer_, area);
}

void LayerManager::Draw(unsigned int id) const {
    bool draw = false;
    Rectangle<int> window_area;
    for(auto layer : layer_stack_) {
        if(layer->ID() == id) {
            window_area.size = layer->GetWindow()->Size();
            window_area.pos = layer->GetPosition();
            draw = true;
        }
        if(draw) { // idより上のlayerを再描画
            layer->DrawTo(back_buffer_, window_area);
        }
    }
    screen_->Copy(window_area.pos, back_buffer_, window_area);
}

void LayerManager::Hide(unsigned int id) {
    auto layer = FindLayer(id);
    auto pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
    if(pos != layer_stack_.end()) {
        layer_stack_.erase(pos);
    }
}

void LayerManager::UpDown(unsigned int id, int new_height) {
    if(new_height < 0) {
        Hide(id);
        return;
    }
    if(new_height > layer_stack_.size()) {
        new_height = layer_stack_.size();
    }
    auto layer = FindLayer(id);
    auto old_pos = std::find(layer_stack_.begin(), layer_stack_.end(), layer);
    auto new_pos = layer_stack_.begin() + new_height;

    if(old_pos == layer_stack_.end()) { // end() returns an iter that point to the next to the last element.  
        layer_stack_.insert(new_pos, layer);
        return;
    }
    if(new_pos == layer_stack_.end()) {  // new_pos == layer_stack.size()
        --new_pos;
    }
    layer_stack_.erase(old_pos);
    layer_stack_.insert(new_pos, layer);
}

std::shared_ptr<Window> Layer::GetWindow() const {
    return window_;
}

Layer* LayerManager::FindLayerByPosition(Vector2D<int> pos, unsigned int exclude_id) const {
   auto pred = [pos, exclude_id](Layer* layer) -> bool {
        if(layer->ID() == exclude_id) {
            return false;
        }
        const auto& win = layer->GetWindow();
        if(!win) {
            return false; 
        }
        const auto win_pos = layer->GetPosition();
        const auto win_end_pos = win_pos + win->Size();
        return win_pos.x <= pos.x && pos.x <= win_end_pos.x &&
               win_pos.y <= pos.y && pos.y <= win_end_pos.y;
   };
   // 上から順番に検索  
   auto it = std::find_if(layer_stack_.rbegin(), layer_stack_.rend(), pred);
   if(it == layer_stack_.rend()) {
        return nullptr;
   }
   return *it;
}

namespace {
    FrameBuffer* screen;
}
  
LayerManager* layer_manager;

void InitializeLayer() {
    const auto screen_size = ScreenSize();

    // 背景ウィンドウの初期化と描画
    auto bgwindow = std::make_shared<Window>(screen_size.x, screen_size.y, screen_config.pixel_format);
    DrawDesktop(*bgwindow->Writer());

    // コンソールウィンドウの初期化と描画
    auto console_window = std::make_shared<Window>(
            Console::kColumns * 8, Console::kRows * 16, screen_config.pixel_format);
    console->SetWindow(console_window);

    // 本物のフレームバッファ用クラスの初期化
    screen = new FrameBuffer; 
    if(auto err = screen->Initialize(screen_config)) { 
        Log(kError, "failed to initialize frame buffer: %s at %s:%d\n",
            err.Name(), err.File(), err.Line());
    }

    layer_manager = new LayerManager;
    layer_manager->SetWriter(screen);

    auto bglayer_id = layer_manager->NewLayer()
        .SetWindow(bgwindow)
        .Move({0, 0})
        .ID();

    auto console_window_layer_id = layer_manager->NewLayer()
        .SetWindow(console_window)
        .Move({0, 0})
        .ID();
    console->SetLayerID(console_window_layer_id);

    layer_manager->UpDown(bglayer_id, 0);
    layer_manager->UpDown(console_window_layer_id, 1);
}