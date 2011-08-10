#include "PCH.h"
#include "SkyConfigurationPane.h"

SkyConfigurationPane::SkyConfigurationPane(Gwen::Controls::Base* parent, SkyPostProcess* pp)
	: ConfigurationPane(parent, L"Sky", pp)
{
	const int labelHeight = 20;

	_skyColorLabel = new Gwen::Controls::Label(this);
	_skyColorLabel->SetText("Sky color:");
	_skyColorLabel->SetAlignment(Gwen::Pos::Bottom | Gwen::Pos::Left);
	_skyColorLabel->SetHeight(labelHeight);
	_skyColorLabel->Dock(Gwen::Pos::Top);

	_skyColorPicker = new Gwen::Controls::ColorPicker(this);
	_skyColorPicker->SetColor(vectorToGwenColor(pp->GetSkyColor()));
	_skyColorPicker->Dock(Gwen::Pos::Top);
	_skyColorPicker->onColorChanged.Add(this, &SkyConfigurationPane::OnValueChanged);

	_sunEnabledCheckBox = new Gwen::Controls::CheckBoxWithLabel(this);
	_sunEnabledCheckBox->Label()->SetText("Sun enabled");
	_sunEnabledCheckBox->Checkbox()->SetChecked(pp->GetSunEnabled());
	_sunEnabledCheckBox->Dock(Gwen::Pos::Top);
	_sunEnabledCheckBox->Checkbox()->onCheckChanged.Add(this, &SkyConfigurationPane::OnValueChanged);

	_sunColorLabel = new Gwen::Controls::Label(this);
	_sunColorLabel->SetText("Sun color:");
	_sunColorLabel->SetAlignment(Gwen::Pos::Bottom | Gwen::Pos::Left);
	_sunColorLabel->SetHeight(labelHeight);
	_sunColorLabel->Dock(Gwen::Pos::Top);
	
	_sunColorPicker = new Gwen::Controls::ColorPicker(this);
	_sunColorPicker->SetColor(vectorToGwenColor(pp->GetSunColor()));
	_sunColorPicker->Dock(Gwen::Pos::Top);
	_sunColorPicker->onColorChanged.Add(this, &SkyConfigurationPane::OnValueChanged);
	
	_sunIntensitySlider = new SliderWithLabel(this);
	_sunIntensitySlider->Slider()->SetRange(0.0f, 25.0f);
	_sunIntensitySlider->Slider()->SetValue(pp->GetSunIntensity());		
	_sunIntensitySlider->Slider()->onValueChanged.Add(this, &SkyConfigurationPane::OnValueChanged);
	_sunIntensitySlider->Dock(Gwen::Pos::Top);

	_sunDirLabel = new Gwen::Controls::Label(this);
	_sunDirLabel->SetAlignment(Gwen::Pos::Bottom | Gwen::Pos::Left);
	_sunDirLabel->SetHeight(labelHeight);
	_sunDirLabel->Dock(Gwen::Pos::Top);

	_sunDirSelector = new DirectionSelector(this);
	XMFLOAT3 sunDir = pp->GetSunDirection();
	_sunDirSelector->SetDirection(XMFLOAT2(sunDir.x, sunDir.z));
	_sunDirSelector->Dock(Gwen::Pos::Top);
	_sunDirSelector->onValueChanged.Add(this, &SkyConfigurationPane::OnValueChanged);
	
	_sunWidthSlider = new SliderWithLabel(this);
	_sunWidthSlider->Slider()->SetRange(EPSILON, 0.5f);
	_sunWidthSlider->Slider()->SetValue(pp->GetSunWidth());		
	_sunWidthSlider->Slider()->onValueChanged.Add(this, &SkyConfigurationPane::OnValueChanged);
	_sunWidthSlider->Dock(Gwen::Pos::Top);
}

SkyConfigurationPane::~SkyConfigurationPane()
{
}

Gwen::Color SkyConfigurationPane::vectorToGwenColor(const XMFLOAT3& vector)
{
	return Gwen::Color(vector.x * 255, vector.y * 255, vector.z * 255);
}

XMFLOAT3 SkyConfigurationPane::gwenColorToVector(const Gwen::Color& color)
{
	return XMFLOAT3(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f);
}

void SkyConfigurationPane::OnValueChanged(Gwen::Controls::Base *control)
{
	SkyPostProcess* pp = GetConfiguredObject();

	if (control == _skyColorPicker)
	{
		pp->SetSkyColor(gwenColorToVector(_skyColorPicker->GetColor()));
	}
	else if (control == _sunEnabledCheckBox->Checkbox())
	{
		pp->SetSunEnabled(_sunEnabledCheckBox->Checkbox()->IsChecked());
	}
	else if (control == _sunColorPicker)
	{
		pp->SetSunColor(gwenColorToVector(_sunColorPicker->GetColor()));
	}
	else if (control == _sunWidthSlider->Slider())
	{
		pp->SetSunWidth(_sunWidthSlider->Slider()->GetValue());
	}
	else if (control == _sunIntensitySlider->Slider())
	{
		pp->SetSunIntensity(_sunIntensitySlider->Slider()->GetValue());
	}
	else if (control == _sunDirSelector)
	{
		XMFLOAT2 dir = _sunDirSelector->GetDirection();
		float y = sqrtf(1.0f - (dir.x * dir.x + dir.y * dir.y));
		pp->SetSunDirection(XMFLOAT3(dir.x, y, dir.y));
	}
}

void SkyConfigurationPane::OnFrameMove(double totalTime, float dt)
{
	SkyPostProcess* pp = GetConfiguredObject();

	_skyColorPicker->SetColor(vectorToGwenColor(pp->GetSkyColor()));
	_sunEnabledCheckBox->Checkbox()->SetChecked(pp->GetSunEnabled());
	_sunColorPicker->SetColor(vectorToGwenColor(pp->GetSunColor()));

	_sunIntensitySlider->Slider()->SetValue(pp->GetSunIntensity());
	_sunIntensitySlider->Label()->SetText("Sun Intensity: " + Gwen::Utility::ToString(pp->GetSunIntensity()));

	_sunWidthSlider->Slider()->SetValue(pp->GetSunWidth());
	_sunWidthSlider->Label()->SetText("Sun width: " + Gwen::Utility::ToString(pp->GetSunWidth()));

	XMFLOAT3 sunDir = pp->GetSunDirection();
	_sunDirSelector->SetDirection(XMFLOAT2(sunDir.x, sunDir.z));
	_sunDirLabel->SetText("Sun direction: (" + Gwen::Utility::ToString(sunDir.x) + 
		", " + Gwen::Utility::ToString(sunDir.y) + ", " + Gwen::Utility::ToString(sunDir.z) + ")");
}