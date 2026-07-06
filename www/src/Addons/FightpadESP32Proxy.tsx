import { useTranslation } from 'react-i18next';
import { FormCheck, Row } from 'react-bootstrap';
import * as yup from 'yup';

import Section from '../Components/Section';
import FormControl from '../Components/FormControl';
import FormSelect from '../Components/FormSelect';
import { AddonPropTypes } from '../Pages/AddonsConfigPage';

const pinSchema = yup.number().required().min(-1).max(47);

export const fightpadESP32ProxyScheme = {
	FightpadESP32ProxyAddonEnabled: yup.number().required(),
	fightpadESP32ProxyUartBlock: yup.number().required().min(0).max(1),
	fightpadESP32ProxyBaud: yup.number().required().min(1),
	fightpadESP32ProxyResetPin: pinSchema,
	fightpadESP32ProxyBootPin: pinSchema,
	fightpadESP32ProxyTxPin: pinSchema,
	fightpadESP32ProxyRxPin: pinSchema,
	fightpadESP32ProxyCtsPin: pinSchema,
	fightpadESP32ProxyRtsPin: pinSchema,
	fightpadESP32ProxyUseFlowControl: yup.number().required(),
	fightpadESP32ProxyAutoDtrRts: yup.number().required(),
};

export const fightpadESP32ProxyState = {
	FightpadESP32ProxyAddonEnabled: 0,
	fightpadESP32ProxyUartBlock: 0,
	fightpadESP32ProxyBaud: 115200,
	fightpadESP32ProxyResetPin: 34,
	fightpadESP32ProxyBootPin: 35,
	fightpadESP32ProxyTxPin: 44,
	fightpadESP32ProxyRxPin: 45,
	fightpadESP32ProxyCtsPin: -1,
	fightpadESP32ProxyRtsPin: -1,
	fightpadESP32ProxyUseFlowControl: 0,
	fightpadESP32ProxyAutoDtrRts: 1,
};

const pinFields = [
	{
		name: 'fightpadESP32ProxyResetPin',
		label: 'fightpad-esp32-proxy-reset-pin-label',
	},
	{
		name: 'fightpadESP32ProxyBootPin',
		label: 'fightpad-esp32-proxy-boot-pin-label',
	},
	{
		name: 'fightpadESP32ProxyTxPin',
		label: 'fightpad-esp32-proxy-tx-pin-label',
	},
	{
		name: 'fightpadESP32ProxyRxPin',
		label: 'fightpad-esp32-proxy-rx-pin-label',
	},
	{
		name: 'fightpadESP32ProxyCtsPin',
		label: 'fightpad-esp32-proxy-cts-pin-label',
	},
	{
		name: 'fightpadESP32ProxyRtsPin',
		label: 'fightpad-esp32-proxy-rts-pin-label',
	},
] as const;

const FightpadESP32Proxy = ({
	values,
	errors,
	handleChange,
	handleCheckbox,
}: AddonPropTypes) => {
	const { t } = useTranslation();

	return (
		<Section title={t('AddonsConfig:fightpad-esp32-proxy-header-text')}>
			<div
				id="FightpadESP32ProxyAddonOptions"
				hidden={!values.FightpadESP32ProxyAddonEnabled}
			>
				<div className="alert alert-info" role="alert">
					{t('AddonsConfig:fightpad-esp32-proxy-sub-header-text')}
				</div>
				<Row className="mb-3">
					<FormSelect
						label={t('AddonsConfig:fightpad-esp32-proxy-uart-block-label')}
						name="fightpadESP32ProxyUartBlock"
						className="form-select-sm"
						groupClassName="col-sm-3 mb-3"
						value={values.fightpadESP32ProxyUartBlock}
						error={errors.fightpadESP32ProxyUartBlock}
						isInvalid={Boolean(errors.fightpadESP32ProxyUartBlock)}
						onChange={handleChange}
					>
						<option value={0}>UART0</option>
						<option value={1}>UART1</option>
					</FormSelect>
					<FormControl
						type="number"
						label={t('AddonsConfig:fightpad-esp32-proxy-baud-label')}
						name="fightpadESP32ProxyBaud"
						className="form-control-sm"
						groupClassName="col-sm-3 mb-3"
						value={values.fightpadESP32ProxyBaud}
						error={errors.fightpadESP32ProxyBaud}
						isInvalid={Boolean(errors.fightpadESP32ProxyBaud)}
						onChange={handleChange}
						min={1}
					/>
				</Row>
				<Row className="mb-3">
					{pinFields.map(({ name, label }) => (
						<FormControl
							key={name}
							type="number"
							label={t(`AddonsConfig:${label}`)}
							name={name}
							className="form-control-sm"
							groupClassName="col-sm-3 mb-3"
							value={values[name]}
							error={errors[name]}
							isInvalid={Boolean(errors[name])}
							onChange={handleChange}
							min={-1}
							max={47}
						/>
					))}
				</Row>
				<Row className="mb-3">
					<div className="col-sm-3 mb-3">
						<FormCheck
							label={t('AddonsConfig:fightpad-esp32-proxy-flow-control-label')}
							type="switch"
							id="FightpadESP32ProxyFlowControl"
							reverse
							checked={Boolean(values.fightpadESP32ProxyUseFlowControl)}
							onChange={(e) => {
								handleCheckbox('fightpadESP32ProxyUseFlowControl');
								handleChange(e);
							}}
						/>
					</div>
					<div className="col-sm-3 mb-3">
						<FormCheck
							label={t('AddonsConfig:fightpad-esp32-proxy-auto-dtr-rts-label')}
							type="switch"
							id="FightpadESP32ProxyAutoDtrRts"
							reverse
							checked={Boolean(values.fightpadESP32ProxyAutoDtrRts)}
							onChange={(e) => {
								handleCheckbox('fightpadESP32ProxyAutoDtrRts');
								handleChange(e);
							}}
						/>
					</div>
				</Row>
			</div>
			<FormCheck
				label={t('Common:switch-enabled')}
				type="switch"
				id="FightpadESP32ProxyAddonButton"
				reverse
				isInvalid={false}
				checked={Boolean(values.FightpadESP32ProxyAddonEnabled)}
				onChange={(e) => {
					handleCheckbox('FightpadESP32ProxyAddonEnabled');
					handleChange(e);
				}}
			/>
		</Section>
	);
};

export default FightpadESP32Proxy;
